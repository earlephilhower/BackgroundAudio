
// PlayAACSD - Dhiru Kholia <dhiru@openwall.com>
// Released to the public domain February 2025.
//
// Plays a AAC file (named demo.aac) from the SD card using the CJMCU-4344
// DAC. This program should help in getting MCLK-enabled DACs to work well.
//
// This example was only tested on a Raspberry Pi Pico 2 board.
//
// SD card support requires https://github.com/earlephilhower/ESP8266SdFat
// library.
//
// Based on PlayAACROM - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.

#include <BackgroundAudioAAC.h>
#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(4, 5, 6); // BCLK, LRCLK, DOUT (,MCLK)
#else
#include <I2S.h>
I2S audio(OUTPUT);
#endif
#include <SPI.h>

#include "SdFat.h"
#include "sdios.h"

// CJMCU-4344 DAC pinout
#define pMCLK 26
#define pBCLK 27
#define pWS (pBCLK + 1)
#define pDOUT 29

#define MCLK_MUL 64  // depends on audio hardware

#define FILENAME_TO_PLAY "demo.aac"
uint8_t filebuff[512];
BackgroundAudioAAC BMP(audio);

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3
const int8_t DISABLE_CHIP_SELECT = -1;
#define SPI_SPEED SD_SCK_MHZ(4)
#if SD_FAT_TYPE == 0
SdFat sd;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// Serial streams
ArduinoOutStream cout(Serial);

#define error(s) sd.errorHalt(&Serial, F(s))

// SD card chip select
int chipSelect = 5;

void setup() {
  Serial.begin(115200);
  Serial.println("PlayAACSD demo program");

#ifdef ARDUINO_ARCH_RP2040
  audio.setBCLK(pBCLK);
  audio.setDATA(pDOUT);
  audio.setMCLK(pMCLK);         // must be run before setFrequency() and i2s.begin()
  audio.setMCLKmult(MCLK_MUL);  // also before i2s.begin()
  audio.setBitsPerSample(16);
  audio.swapClocks();  // required for CJMCU-4344 DAC

  // Follows https://github.com/elehobica/RPi_Pico_WAV_Player?tab=readme-ov-file#sd-card-interface
  SPI.setSCK(2);
  SPI.setTX(3);
  SPI.setRX(4);
  SPI.setCS(chipSelect);
  SPI.begin(false);
#endif

  if (!sd.begin(chipSelect, SPI_SPEED)) {
    if (sd.card()->errorCode()) {
      cout << F(
             "\nSD initialization failed.\n");
      cout << F("\nerrorCode: ") << hex << showbase;
      cout << int(sd.card()->errorCode());
      cout << F(", errorData: ") << int(sd.card()->errorData());
      cout << dec << noshowbase << endl;
      return;
    }
    cout << F("\nCard successfully initialized.\n");
    if (sd.vol()->fatType() == 0) {
      cout << F("Can't find a valid FAT16/FAT32 partition.\n");
      return;
    }
    cout << F("Can't determine error type\n");
    return;
  }
  cout << F("Files found (date time size name):\n");
  sd.ls(LS_R | LS_DATE | LS_SIZE);

  if (!file.open(FILENAME_TO_PLAY, FILE_READ)) {
    error("SD file open failed");
  }

  BMP.begin();
}

void loop() {
  while (file && BMP.availableForWrite() > 512) {
    int len = file.read(filebuff, 512);
    BMP.write(filebuff, len);
  }
}
