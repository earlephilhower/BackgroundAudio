// Released to the public domain, Earle F. Philhower, III <earlephilhower@yahoo.com>
// Simple test to measure the CPU load of different playback codecs and optimizations.
// Generates same number of audio samples (playback time) with each codes, but because
// HE AAC generates 2048 samples per frame and MP3 generates 1152 samples, we
// run 103 frames AAC and 178 frames MP3.  The playback time is essentially identical
// with these settings and the complexity difference in HE AAC vs. MP3 is obvious.
// Useful to see how effective different -Ox optimizations are and different CPU archs.

// Note that this is NOT how you use the library!  Only for low-level performance testing

#include <BackgroundAudio.h>
#include "aac.h"
#include "aache.h"
#include "mp3.h"

HAACDecoder _hAACDecoder;
int16_t _outSample[2048][2];

struct mad_stream _stream;
struct mad_frame _frame;
struct mad_synth _synth;

uint64_t cyclesAAC;
uint64_t cyclesAACHE;
uint64_t cyclesMP3;
uint32_t aacframes = 0;
uint32_t aacheframes = 0;
uint32_t mp3frames = 0;

#ifdef ESP32
#define rp2040 ESP
#define getCycleCount64 getCycleCount
#endif

void setup() {
  Serial.begin(115200);
  _hAACDecoder = AACInitDecoder();
  int idx = 0;
  uint64_t now = rp2040.getCycleCount64();
  while ((idx < (int)sizeof(aac)) && (aacframes < 206)) {
    int nextFrame = AACFindSyncWord((uint8_t *)&aac[idx], sizeof(aac) - idx);
    idx += nextFrame;
    int bytesLeft = sizeof(aac) - idx;
    const unsigned char *inBuff = aac + idx;
    if (!AACDecode(_hAACDecoder, (unsigned char **)&inBuff, &bytesLeft, (int16_t *)_outSample)) {
      AACFrameInfo fi;
      AACGetLastFrameInfo(_hAACDecoder, &fi);
      aacframes++;
      idx += sizeof(aac) - idx - bytesLeft;
    } else {
      idx = sizeof(aac);
      Serial.printf("err %lu\r\n", aacframes);
    }
  }
  uint64_t done = rp2040.getCycleCount64();
  cyclesAAC = done - now;
  AACFreeDecoder(_hAACDecoder);


  _hAACDecoder = AACInitDecoder();
  idx = 0;
  now = rp2040.getCycleCount64();
  while ((idx < (int)sizeof(aache)) && (aacheframes < 103)) {
    int nextFrame = AACFindSyncWord((uint8_t *)&aache[idx], sizeof(aache) - idx);
    idx += nextFrame;
    int bytesLeft = sizeof(aache) - idx;
    const unsigned char *inBuff = aache + idx;
    if (!AACDecode(_hAACDecoder, (unsigned char **)&inBuff, &bytesLeft, (int16_t *)_outSample)) {
      AACFrameInfo fi;
      AACGetLastFrameInfo(_hAACDecoder, &fi);
      aacheframes++;
      idx += sizeof(aache) - idx - bytesLeft;
    } else {
      idx = sizeof(aache);
      Serial.printf("err %lu\r\n", aacheframes);
    }
  }
  done = rp2040.getCycleCount64();
  cyclesAACHE = done - now;
  AACFreeDecoder(_hAACDecoder);

  mad_stream_init(&_stream);
  mad_frame_init(&_frame);
  mad_synth_init(&_synth);
  mad_stream_options(&_stream, 0);
  idx = 0;
  now = rp2040.getCycleCount64();
  while ((idx < (int)sizeof(mp3)) && (mp3frames < 183)) {
    if (_stream.buffer) {
      if (_stream.next_frame && _stream.next_frame != _stream.this_frame) {
        idx += _stream.next_frame - _stream.this_frame;
      } else {
        // We have only a partial frame in the buffer.  We need data *now* do dump it and send zeros.  Underflow case
        idx += 2881;
      }
    }
    int ptr = 0;
    int pend = sizeof(mp3) - 1 - idx;
    const uint8_t *b = mp3 + idx;
    bool found = false;
    for (ptr = 0; ptr < pend; ptr++) {
      if ((b[ptr] == 0xff) && ((b[ptr + 1] & 0xe0) == 0xe0)) {
        idx += ptr;
        found = true;
        break;
      }
    }
    if (found) {
      mad_stream_buffer(&_stream, mp3 + idx, sizeof(mp3) - idx);
      if (!mad_frame_decode(&_frame, &_stream)) {
        mad_synth_frame(&_synth, &_frame);
        mp3frames++;
      }
    }
  }
  done = rp2040.getCycleCount64();
  cyclesMP3 = done - now;
}

void report(const char *name, int frames, uint64_t cyclesttl, int samplesperframe) {
  double cyclesPerSample = cyclesttl / (double)(frames * samplesperframe);
  double mhzUsed = (44100.0 * cyclesPerSample) / 1000000.0;
  Serial.printf("%s decode cycles: %llu, frames %d, cycles/sample %0.2f, MHZ %0.0f\r\n", name, cyclesttl, frames, cyclesPerSample, mhzUsed);
}

void loop() {
  report("HE-AAC", aacheframes, cyclesAACHE, 2048);
  report("AAC", aacframes, cyclesAAC, 1024);
  report("MP3", mp3frames, cyclesMP3, 1152);
  Serial.println("-------------------------------------\r\n");
  delay(1000);
}
