
// PlayAACSD - Dhiru Kholia <dhiru@openwall.com>
// Released to the public domain February 2025.
//
// Plays an AAC file (named demo.aac) from the SD card using the CJMCU-4344
// DAC. This program should help in getting MCLK-enabled DACs to work well.
// This example also works well with the ESHINE CJC4334 DAC.
//
// This example should also work for the Pico-Audio Rev2.1 DAC which uses
// the CS4344 DAC chip as well.
//
// This example was only tested on a Raspberry Pi Pico 2 board.
//
// Based on PlayAACROM + NotSoSimpleMP3Shuffle examples by Earle F. Philhower, III

#include <BackgroundAudioAAC.h>
#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(4, 5, 6);  // BCLK, LRCLK, DOUT (,MCLK), untested
#else
#include <I2S.h>
I2S audio(OUTPUT);
#endif
#include <SD.h>

// CJMCU-4344 DAC pinout
#define pMCLK 26
#define pLRCLK 27
#define pDOUT 29

#define MCLK_MUL 64  // depends on the audio hardware

#define FILENAME_TO_PLAY "demo.aac"
File f;
uint8_t filebuff[512];
BackgroundAudioAAC BMP(audio);

// SD card chip select
int chipSelect = 5;

void fail() {
  while (1) {
    delay(1000);
#ifdef ESP32
    ESP.restart();
#else
    rp2040.reboot();
#endif
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("PlayAACSD demo program");

#ifdef ARDUINO_ARCH_RP2040
  audio.setBCLK(pLRCLK);
  audio.setDATA(pDOUT);
  audio.setMCLK(pMCLK);         // must be run before setFrequency() and i2s.begin()
  audio.setMCLKmult(MCLK_MUL);  // also before i2s.begin()
  // In swapClocks mode the setBCLK call actually sets LRCLK
  audio.swapClocks();  // required for CJMCU-4344 DAC

  // Follows https://github.com/elehobica/RPi_Pico_WAV_Player?tab=readme-ov-file#sd-card-interface
  SPI.setSCK(2);
  SPI.setTX(3);
  SPI.setRX(4);
  SPI.setCS(chipSelect);
  SPI.begin(false);
#endif

  // Initialize the SD card
  bool sdInitialized = false;
#ifdef ESP32
  sdInitialized = SD.begin(chipSelect);
#else
  sdInitialized = SD.begin(chipSelect);
#endif

  if (!sdInitialized) {
    Serial.println("initialization failed!");
    fail();
  }

  f = SD.open(FILENAME_TO_PLAY);
  if (!f) {
    Serial.printf("Unable to open %s", FILENAME_TO_PLAY);
    fail();
  }

  BMP.begin();
}

void loop() {
  while (f && BMP.availableForWrite() > 512) {
    int len = f.read(filebuff, 512);
    BMP.write(filebuff, len);
    if (len != 512) {
      f.close();
    }
  }
}
