// PlayAACROM - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.
//
// Plays a small AAC file from ROM asynchronously with a single call.
// Hook up an I2S DAC bclk=0; lrclk=1, data=2
//
// Generate the ROM file by using "xxd -i file.aac file.h" and then
// editing the output header to make the array "const" so it stays
// only in flash.
//
// Intended as a simple demonstration of BackgroundAudio usage.

#include <BackgroundAudioAAC.h>
#include <__example_pianoaac.h>

#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio i2s(4, 5, 6); // BCLK, LRCLK, DOUT (,MCLK)
#else
#include <I2S.h>
I2S i2s(OUTPUT, 0, 2);
#endif
ROMBackgroundAudioAAC BMP(i2s);

bool stopped = false;

void setup() {
  Serial.begin(115200);
  delay(5000);

  // Signal we've started
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Point player to our ROM MP3
  BMP.write(piano_aac, sizeof(piano_aac));
  // Start the background player
  BMP.begin();
}

void loop() {
  if (!BMP.done()) {
    Serial.printf("Buffer: %d -- %lu %lu %lu %lu -- ", BMP.available(), BMP.shifts(), BMP.frames(), BMP.errors(), BMP.dumps());
    Serial.printf("Waiting to finish....could be doing something useful here\r\n");
    delay(100);
  } else if (!stopped) {
    stopped = true;
    digitalWrite(LED_BUILTIN, LOW);
    Serial.printf("done\r\nHit enter to go again\r\n");
  }
  if (Serial.available()) {
    while (Serial.available()) {
      Serial.read(); // Throw it away for this example
    }
    stopped = false;
    Serial.printf("\r\nPlaying again\r\n");
    BMP.write(piano_aac, sizeof(piano_aac));
  }
}
