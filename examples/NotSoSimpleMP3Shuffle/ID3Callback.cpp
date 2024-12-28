/*
    NotSoSimpleMP3Shuffle - ID3 Callback

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

#include "ID3.h"
#include "ID3Callback.h"

// See https://id3lib.sourceforge.net/id3/develop.html for full definitions
// In a real embedded application, only interesting fields would be verified
// and they would set global variables for the UI to update on the next loop()
// Here, just print to USB.
void id3Callback(int vers, char *field, int encoding, char *value, int valueLen) {
  uint32_t f32 = *(uint32_t*)field;
  char lang[4] = {value[0], value[1], value[2], 0}; // Not always used, but always same format
  char *desc;
  char *text;
  char *mime;
  char *name;

  switch (f32) {
    case M("COMM"):
    case M("COM"):
      desc = value + 3;
      if ((encoding == 1) || (encoding == 2)) {
        text = desc + myu16strlen(desc) + 2;
      } else {
        text = desc + strlen(desc) + 1;
      }
      Serial.printf("Comment: %s[%s] = ", toUTF8(encoding, desc, myu16strlen(desc)), lang);
      Serial.printf("%s\n", toUTF8(encoding, text, myu16strlen(text)));
      break;
    case M("TXX"):
    case M("TXXX"):
      desc = value;
      if ((encoding == 1) || (encoding == 2)) {
        text = desc + myu16strlen(desc) + 2;
      } else {
        text = desc + strlen(desc) + 1;
      }
      Serial.printf("User Defined: %s = ", toUTF8(encoding, desc, myu16strlen(desc)));
      Serial.printf("%s\n", toUTF8(encoding, text, myu16strlen(text)));
      break;
    case M("PRIV"):
      Serial.printf("Private Field: %s=", value);
      valueLen -= strlen(value) + 1;
      value += strlen(value) + 1;
      //while (valueLen--) {
      //  Serial.printf("%02X", *(value++));
      //}
      Serial.printf("\n");
      break;
    case M("GEO"):
    case M("GEOB"):
      mime = value;
      valueLen -= strlen(mime) + 1;
      name = mime + strlen(mime) + 1;
      valueLen -= strlen(name) + 1;
      desc = name + strlen(name) + 1;
      valueLen -= strlen(desc) + 1;
      value = desc + strlen(desc) + 1;
      Serial.printf("Object: %s %s = %s, data = ", mime, name, desc);
      //while (valueLen--) {
      //  Serial.printf("%02X", *(value++));
      //}
      Serial.printf("\n");
      break;
    case M("USLT"):
    case M("ULT") :
      desc = value + 3;
      if ((encoding == 1) || (encoding == 2)) {
        text = desc + myu16strlen(desc) + 2;
      } else {
        text = desc + strlen(desc) + 1;
      }
      Serial.printf("Unsynchronized Lyrics: %s[%s] = ", toUTF8(encoding, desc, myu16strlen(desc)), lang);
      Serial.printf("%s\n", toUTF8(encoding, text, myu16strlen(text)));
      break;
    case M("MCI"): // Reach out and touch someone for less
    case M("MCDI"):
      Serial.printf("CDID: ");
      //while (valueLen--) {
      //  Serial.printf("%02X", *(value++));
      //}
      Serial.printf("\n");
      break;
    // PIC and APIC have same meaning, different format
    case M("PIC"):
      Serial.printf("Attached Picture: %s (%d) %s", lang, value[4], value + 5);
      valueLen -= 4;
      value += 4; // Skip lang, value, 
      valueLen -= strlen(value) + 1;
      value += strlen(value) + 1;
      //while (valueLen--) {
      //  Serial.printf("%02X", *(value++));
      //}
      Serial.printf("\n");
      break;
    case M("APIC"):
      Serial.printf("Attached Picture: %s", value);
      valueLen -= strlen(value) + 1;
      value += strlen(value) + 1;
      Serial.printf("(%02x)", value[0]);
      valueLen--;
      value++;
      Serial.printf("%s = ", value); // desc
      valueLen -= strlen(value) + 1;
      value += strlen(value);
      //while (valueLen--) {
      //  Serial.printf("%02X", *(value++));
      //}
      Serial.printf("\n");
      break;
    case M("TALB"):
    case M("TAL"):
      Serial.printf("Album: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TPE1"):
    case M("TP1"):
      Serial.printf("Artist: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TPE2"):
    case M("TP2"):
      Serial.printf("Band: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TPE3"):
    case M("TP3"):
      Serial.printf("Conductor: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TIT2"):
    case M("TT2"):
      Serial.printf("Title: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TENC"):
    case M("TEN"):
      Serial.printf("Encoded By: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TDRC"):
      Serial.printf("Recording Time: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TLEN"):
      Serial.printf("Length(ms): %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TDRL"):
    case M("TRD"):
      Serial.printf("Release Time: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TDAT"):
    case M("TDA"):
      Serial.printf("Release Date: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TPUB"):
    case M("TPB"):
      Serial.printf("Publisher: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TCOM"):
    case M("TCM"):
      Serial.printf("Composer: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TLAN"):
    case M("TLA"):
      Serial.printf("Language: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TFLT"):
    case M("TFT"):
      Serial.printf("File Type: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TYER"):
    case M("TYE"):
      Serial.printf("Year: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TRCK"):
    case M("TRK"):
      Serial.printf("Track: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TCON"):
    case M("TCO"):
      Serial.printf("Content Type: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TPOS"):
    case M("TPA") :
      Serial.printf("Part of Set: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TSSE"):
    case M("TSS"):
      Serial.printf("Encoder: %s\n", toUTF8(encoding, value, valueLen));
      break;
    case M("TBPM"):
    case M("TBP"):
      Serial.printf("BPM: %s\n", toUTF8(encoding, value, valueLen));
      break;
    default:
      Serial.printf("**** %d: %s(%d): %s\n", vers, field, encoding, toUTF8(encoding, value, valueLen));
      break;
  }
}
