// PlayWAV - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.
//
// When BOOTSEL preseed, plays a small WAV file from ROM
// asynchronously with a single call.
// Hook up an earphone to pins 0, 1, and GND to hear the PWM output.
//
// Generate the ROM file by using "xxd -i file.wav file.h" and then
// editing the output header to make the array "const" so it stays
// only in flash.
//
// Intended as a simple demonstration of BackgroundAudio usage.

#include <BackgroundAudioWAV.h>
#include <PWMAudio.h>
#include "wav.h"

PWMAudio pwm(0);
ROMBackgroundAudioWAV BMP(pwm);

void setup() {
  // Signal we've started
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Start the background player
  BMP.begin();
}

uint32_t last = 0;
void loop() {
  if (BOOTSEL) {
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.write(wav, sizeof(wav));
    while (BOOTSEL) {
      /* wait for button release */
      delay(1);
    }
    Serial.printf("BEEP!\r\n");
  }
  if (millis() - last > 1000) {
    Serial.printf("Runtime: %lu\r\n", millis());
    last = millis();
  }
}
