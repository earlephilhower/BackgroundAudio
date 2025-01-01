// Mixer - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.
//
// Plays an AAC from ROM to the speaker and overlays "beep" when
// BOOTSEL is pressed.
// Hook up an earphone to pins 0, 1, and GND to hear the PWM output.
//
// Generate the ROM file by using "xxd -i file.wav file.h" and then
// editing the output header to make the array "const" so it stays
// only in flash.
//
// Intended as a simple demonstration of BackgroundAudio usage.

#include <BackgroundAudioWAV.h>
#include <BackgroundAudioAAC.h>
#include <BackgroundAudioMixer.h>
#include <PWMAudio.h>
#include <__example_beepwav.h>
#include <__example_pianoaac.h>

PWMAudio pwm(0);
BackgroundAudioMixer<640> mixer(pwm, 44100);
ROMBackgroundAudioWAV wav;
ROMBackgroundAudioAAC aac;

void setup() {
  // Signal we've started
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  wav.setDevice(mixer.add());
  aac.setDevice(mixer.add());

  // Start the background player
  mixer.begin();
  wav.begin();
  aac.begin();
  aac.write(piano_aac, sizeof(piano_aac));
}

uint32_t last = 0;
void loop() {
  if (BOOTSEL) {
    wav.flush();  // Stop any existing output, reset for new file
    wav.write(beepwav, sizeof(beepwav));
    Serial.printf("BEEP!\r\n");
    while (BOOTSEL) {
      /* wait for button release */
      delay(1);
    }
  }
  if (millis() - last > 1000) {
    Serial.printf("Runtime: %lu\r\n", millis());
    last = millis();
  }
}
