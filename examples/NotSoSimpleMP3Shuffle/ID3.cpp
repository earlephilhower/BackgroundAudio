/*
    NotSoSimpleMP3Shuffle - ID3 Processing

    Turns the Pico into a basic MP3 shuffle player and plays all the MP3s
    in the root directory of an SD card.  Hook up an earphone to pins 0, 1,
    and GND to hear the PWM output.  Wire up an SD card to the pins specified
    below.

    Copyright (c) 2024 Earle F. Philhower, III <earlephilhower@yahoo.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include <FS.h>
#include <locale>
#include <string>
#include <codecvt>
#include "ID3.h"

// ID3 tags can have an "unsync" encoding where they are altered not to
// match an MP3 synchronization frame.  This class wraps a File to
// transparently convert an "unsynchronized" tag to a normal one.

class UnsyncFile {
public:
  UnsyncFile(File f, bool unsync, size_t len) : _f(f), _unsync(unsync), _len(len), _pos(0) { }
  ~UnsyncFile() {}
  
  inline int read() {
    if (_pos >= _len) {
      return -1;
    }
    auto r = _f.read();
    if (r < 0) {
      return r;
    }
    if (!_unsync) {
      _pos++;
      return r;
    } else {
      if (r != 0xff) {
        _pos++;
        return r;
      }
      auto z = _f.peek();
      if (z == 0) {
        z = _f.read();
        _pos += 2;
        return r;
      } else {
        _pos++;
        return r;
      }
    }
  }
  
  inline int read(void *dest, size_t cnt) {
    uint8_t *d = (uint8_t *)dest;
    for (size_t i = 0; i < cnt; i++) {
      int ret = read();
      if (ret < 0) {
        return i;
      }
      d[i] = (uint8_t)ret;
    }
    return cnt;
  }

  inline uint32_t read32(int bits = 8) {
    uint8_t b[4];
    if (read(b, 4) != 4) {
      return 0;
    }
    return ((uint32_t)b[3]) | ((uint32_t)b[2] << bits) | ((uint32_t)b[1] << (bits * 2)) | ((uint32_t)b[0] << (bits * 3));
  }

  operator bool() {
    return _pos < _len;
  }

private:
  File _f;
  bool _unsync;
  size_t _len;
  size_t _pos;
};

static char value[4096]; // ID3 tag frame storage, in the heap to allow it to be large

void processID3(File f, void(*cb)(int vers, char *field, int encoding, char *value, int valueLen)) {
  uint8_t hdr[10];
  int vers = 0;
  bool unsync = false;
  bool extendedhdr = false;
  size_t size = 0;

  size_t here = f.position();
  if (10 != f.read(hdr, 10)) {
    f.seek(here);
    return; // empty file?
  }
  if (0x80 & (hdr[6] | hdr[7] | hdr[8] | hdr[9])) {
    // Error, the size must have 0 MSBs
    f.seek(here);
    return;
  }
  const uint8_t id3v220[] = { 'I', 'D', '3', 0x02, 0x00 };
  const uint8_t id3v230[] = { 'I', 'D', '3', 0x03, 0x00 };
  const uint8_t id3v240[] = { 'I', 'D', '3', 0x04, 0x00 };
  size = ((size_t)hdr[9]) | ((size_t)hdr[8] << 7) | ((size_t)hdr[7] << 14) | ((size_t)hdr[6] << 21);
  if (!memcmp(hdr, id3v220, sizeof(id3v220))) {
    vers = 2;
    unsync = hdr[5] & 0x80;
    extendedhdr = false;
  } else if (!memcmp(hdr, id3v230, sizeof(id3v230))) {
    vers = 3;
    unsync = hdr[5] & 0x80;
    extendedhdr = hdr[5] & 0x40;
  } else if (!memcmp(hdr, id3v240, sizeof(id3v240))) {
    vers = 4;
    unsync = hdr[5] & 0x80;
    extendedhdr = hdr[5] & 0x40;
  }
  if (!vers) {
    f.seek(here);
    return;
  }
  UnsyncFile u(f, unsync, size);

  if (extendedhdr) {
    if (vers == 3) {
      uint32_t xlen = u.read32();
      // Skip flags
      u.read();
      u.read();
      if (xlen > 6) {
        // Skip padding, it's 0-filled and will be caught by the frameid/size checks
        u.read32();
      }
    } else if (vers == 4) {
      // Skip extlen
      u.read32(7);
      // skip flaglen
      u.read();
      // skip flags
      u.read();
    }
  }

  // Now in frames, parse 
  while (u) {
    if (vers == 2) {
      uint8_t frameid[4] = {};
      uint8_t s[3];
      int size;
      uint8_t encoding = 0;
      
      if (3 != u.read(frameid, 3)) {
        break;
      }
      if (3 != u.read(s, 3)) {
        break;
      }
      size = ((uint32_t)s[2]) | ((uint32_t)s[1] << 8) | ((uint32_t)s[0] << 16) ;
      if (size) {
        if ((frameid[0] == 'T') || !memcmp(frameid, "COM", 3)) {
          encoding = u.read();
          size--;
        }
        int toRead = std::min((size_t)size, sizeof(value) - 1);
        if (toRead != u.read(value, toRead)) {
          break;
        }
        value[toRead] = 0;
        cb(vers, (char *)frameid, encoding, value, toRead);
        size -= toRead;
        while (size) {
          if (u.read() < 0) {
            break;
          }
        }
      } else {
        break;
      }
    } else if ((vers == 3) || (vers == 4)) {
      uint8_t frameid[5] = {};
      uint32_t size;
      uint8_t flags[2];
      uint8_t encoding = 0;
      if (4 != u.read(frameid, 4)) {
        break;
      }
      size = u.read32(vers == 4 ? 7 : 8);
      u.read(flags, 2);
      if ( (frameid[0] == 'T') ||
           !memcmp(frameid, "COMM", 4) || !memcmp(frameid, "SYLT", 4) || !memcmp(frameid, "USLT", 4) ||
           !memcmp(frameid, "GEOB", 4) || !memcmp(frameid, "APIC", 4) || !memcmp(frameid, "WXXX", 4) ||
           !memcmp(frameid, "IPLS", 4) || !memcmp(frameid, "ETCO", 4) || !memcmp(frameid, "SYTC", 4) ||
           !memcmp(frameid, "POSS", 4) || !memcmp(frameid, "USER", 4) || !memcmp(frameid, "OWNE", 4) ||\
           !memcmp(frameid, "COMR", 4) ) {
        encoding = u.read();
        size--;
      }
      if (size) {
        int toRead = std::min((size_t)size, sizeof(value) - 2);
        if (toRead != u.read(value, toRead)) {
          break;
        }
        value[toRead] = 0;
        value[toRead + 1] = 0;
        cb(vers, (char *)frameid, encoding, value, toRead);
        size -= toRead;
        while (size) {
          if (u.read() < 0) {
            break;
          }
        }
      } else {
        break; // Error or padding
      }
    }
  }

  // Potential padding
  while (u) {
    u.read();
  }
}

static char _utf8buff[4096]; // UTF16->UTF8 temporary buffer, on heap so it can be big
static constexpr uint32_t INVALID = 0xfffd; // U+FFFD � REPLACEMENT CHARACTER

// Convert from UTF-16 to UTF-32 with very basic error checking
static inline uint32_t from16(uint16_t const* utf16, size_t len, size_t* index, bool be) {
  uint8_t *ptr = (uint8_t *)utf16; // All reads of the u16 chars will be byte-wide to allow misalignment
  uint16_t high = be ? ptr[*index * 2] | (ptr[*index * 2 + 1] << 8) : ptr[*index * 2 + 1] | (ptr[*index * 2] << 8);

  if (!(high & 0xf800)) {
    // This is a short char, fall through
    return high;
  }
  if ((high & 0xfc00) != 0xd800) {
    // Not a high half?!
    return INVALID;
  }
  if (*index == len - 1) {
    // can't read more but need more
    return INVALID;
  }

  (*index)++; // We ate the 1st char, this next one implied
  
  uint16_t low = be ? ptr[(*index + 1) * 2] | (ptr[(*index + 1) * 2 + 1] << 8) : ptr[(*index + 1) * 2 + 1] | (ptr[(*index + 1) * 2] << 8);
  // Better be a low half marker
  if ((low & 0xfc00) != 0xdc00) {
    return INVALID;
  }
  uint32_t result;
  result = high & ((1<<10) - 1);
  result <<= 10;
  result |= low & ((1<<10) - 1);
  result |= 0x10000; // Implied bit
  
  // And if all else fails, it's valid
  return result;
}

// Convert UTF-32 to a potentially multibyte UTF-8 encode
static inline size_t to8(uint32_t u32, uint8_t* utf8, size_t len, size_t index) {
  if (u32 <= 0x7f) {
    if (index + 1 >= len) {
      return 0;
    }
    utf8[index] = u32; // Already know 0-7F
    return 1;
  } else if (u32 <= 0x7ff) {
    if (index + 2 >= len) {
      return 0;
    }
    utf8[index] = (0x80 + 0x40) | u32 >> 6;
    utf8[index + 1] = 0x80 | (u32 & ~0xc0);
    return 2;
  } else if (u32 <= 0xffff) {
    if (index + 3 >= len) {
      return 0;
    }
    utf8[index] = (0x80 + 0x40 + 0x20) | (u32 >> 12);
    utf8[index + 1] = 0x80 | ((u32 >> 6) & ~0xc0);
    utf8[index + 2] = 0x80 | ((u32 >> 0) & ~0xc0);
    return 3;
  } else if (u32 <= 0x10ffff) {
    if (index + 4 >= len) {
      return 0;
    }
    utf8[index] = (0x80 + 40 + 20 + 10) | (u32 >> 18);
    utf8[index + 1] = 0x80 | ((u32 >> 12) & ~0xc0);
    utf8[index + 2] = 0x80 | ((u32 >> 6) & ~0xc0);
    utf8[index + 3] = 0x80 | ((u32 >> 0) & ~0xc0);
    return 4;
  } else {
    return 0;
  }
}

const char *toUTF8(int encoding, const char *str, int strBytes) {
  if ((encoding == 0) || (encoding == 3)) {
    return str;
  } else if ((encoding == 1) || (encoding == 2)) {
    const uint16_t *utf16 = (const uint16_t *)str;
    size_t utf16_len = strBytes / 2;
  
    // Easy-peasy
    if (!utf16_len) {
      _utf8buff[0] = 0;
      return _utf8buff;
    }

    // Silently detect and eat BOM...yum
    bool be;
    if (*str == 0xff && *(str + 1) == 0xfe) {
      be = true;
      utf16_len--;
      utf16++;
    } else if (*str == 0xfe && *(str + 1) == 0xff) {
      be = false;
      utf16_len--;
      utf16++;
    } else {
      be = true;
    }

    size_t utf8_index = 0;
    for (size_t utf16_index = 0; utf16_index < utf16_len; utf16_index++) {
      uint32_t u32 = from16(utf16, utf16_len, &utf16_index, be);
      if (u32 == 0) {
        // End of string detected
        break;
      }
      utf8_index += to8(u32, (uint8_t*)_utf8buff, sizeof(_utf8buff), utf8_index);
    }
    _utf8buff[utf8_index] = 0;
    return _utf8buff;
  } else {
    return "unk";
  }
}


int myu16strlen(const char *ptr) {
  int cnt = 0;
  // Do it byte-wise because we don't want to need to align inputs
  while (*ptr || *(ptr + 1)) {
    cnt++;
    ptr++;
    cnt++;
    ptr++;
  }
  return cnt;
}
