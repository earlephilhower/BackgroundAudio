// SpeakGame - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.
//
// Measures time it takes from when LED is enabled to button or Serial is presses
// Speaks the results for headless play.
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
  digitalWrite(LED_BUILTIN, LOW);

  // Start the background player
  BMP.setVoice(voice_en);
  BMP.begin();
}

void loop() {
  delay(random(3000) + 3000); // Wait a random time over 3 seconds...
  uint32_t now = millis();
  digitalWrite(LED_BUILTIN, HIGH);

#ifdef ESP32
  while (!Serial.available()) {
    delay(1);
  }
  uint32_t press = millis();
  while (Serial.available()) {
    Serial.read(); // Throw out any data
  }
#else
  while (!BOOTSEL) {
    delay(1);
  }
  uint32_t press = millis();
#endif
  char buff[64];
  sprintf(buff, "You took %0.3f seconds to react.", (press - now) / 1000.0);
  BMP.speak(buff);
  // Wait for speaking to finish...could do other things here...
  while (!BMP.done()) {
    delay(1);
  }
  digitalWrite(LED_BUILTIN, LOW);
}
