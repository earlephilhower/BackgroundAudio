// BeepSpeak - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain January 2025.
//
// When BOOTSEL preseed, say "beep" using speech synthesis
// Hook up an earphone to pins 0, 1, and GND to hear the PWM output.
//
// Intended as a simple demonstration of BackgroundAudio usage.

#include <BackgroundAudioSpeech.h>
#include <libespeak-ng/voice/en.h>

#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(4, 5, 6); // BCLK, LRCLK, DOUT (,MCLK)
#else
#include <PWMAudio.h>
PWMAudio audio(0);
#endif
BackgroundAudioSpeech BMP(audio);

void setup() {
  Serial.begin(115200);

  // Signal we've started
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Start the background player
  BMP.setVoice(voice_en);
  BMP.begin();
}

uint32_t last = 0;
void loop() {
#ifdef ESP32
  if (millis() - last > 3500) {
    Serial.printf("Runtime: %lu, %d\r\n", millis(), audio.frames());
    last = millis();
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.speak("beep boop");
  }
#else
  if (BOOTSEL) {
    BMP.flush(); // Stop any existing output, reset for new file
    BMP.speak("beep boop");
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
#endif
}
