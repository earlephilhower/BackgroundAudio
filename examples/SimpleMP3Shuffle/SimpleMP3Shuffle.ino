// SimpleMP3Shuffle - Earle F. Philhower, III <earlephilhower@yahoo.com>
// Released to the public domain December 2024.
//
// Turns the Pico into a basic MP3 shuffle player and plays all the MP3s
// in the root directory of an SD card.  Hook up an earphone to pins 0, 1,
// and GND to hear the PWM output.  Wire up an SD card to the pins specified
// below.
//
// Intended as a simple demonstration of BackgroundAudio usage.

#include <BackgroundAudio.h>
#include <PWMAudio.h>
#include <I2S.h>
#include <SD.h>
#include <vector>
#include <string.h>

// This are GP pins for SPI0 on the Raspberry Pi Pico board, and connect
// to different *board* level pinouts.  Check the PCB while wiring.
// Only certain pins can be used by the SPI hardware, so if you change
// these be sure they are legal or the program will crash.
// See: https://datasheets.raspberrypi.com/picow/PicoW-A4-Pinout.pdf
const int _MISO = 4;  // AKA SPI RX
const int _MOSI = 7;  // AKA SPI TX
const int _CS = 5;
const int _SCK = 6;

// I2S out(OUTPUT, 0, 2);
PWMAudio audio(0);
BackgroundAudioMP3 BMP(audio);

// List of all MP3 files in the root directory
std::vector<String> mp3list;

// The file we're currently playing
File f;

// Read buffer that's better off not in the stack due to its size
uint8_t filebuff[512];

void setup() {
  delay(5000);

  // Initialize the SD card
  bool sdInitialized = false;
  if (_MISO == 0 || _MISO == 4 || _MISO == 16) {
    SPI.setRX(_MISO);
    SPI.setTX(_MOSI);
    SPI.setSCK(_SCK);
    sdInitialized = SD.begin(_CS);
  } else if (_MISO == 8 || _MISO == 12) {
    SPI1.setRX(_MISO);
    SPI1.setTX(_MOSI);
    SPI1.setSCK(_SCK);
    sdInitialized = SD.begin(_CS, SPI1);
  } else {
    Serial.println(F("ERROR: Unknown SPI Configuration"));
    while (1) {
      delay(1);
    }
  }
  if (!sdInitialized) {
    Serial.println("initialization failed!");
    while (1) {
      delay(1);
    }
  }

  // Scan the card and make a list of all MP3 files
  File root = SD.open("/");
  while (true) {
    f = root.openNextFile();
    if (!f) {
      break;
    }
    String n = f.name();
    n.toLowerCase();
    if (strstr(n.c_str(), ".mp3")) {
      mp3list.push_back(n);
    }
    f.close();
  }
  root.close();

  // Start the background player
  BMP.begin();
}

void loop() {
  // When BOOTSEL held, skip to another song
  if (BOOTSEL) {
    f.close();  // Choose another
    // wait for release
    while (BOOTSEL) {
      delay(1);
    }
  }

  // Choose a song from the list if there's no open file
  if (!f) {
    auto x = random(mp3list.size());
    f = SD.open(mp3list[x]);
    Serial.printf("\r\n\r\nNow playing: %s\r\n", mp3list[x].c_str());
  }

  // Stuff the buffer with as much as it will take, only doing full sector reads for performance
  while (f && BMP.availableForWrite() > 512) {
    int len = f.read(filebuff, 512);
    BMP.write(filebuff, len);
    if (len != 512) {
      f.close(); // Short reads == EOF
    }
  }

  // Could do things like draw a UI on a LCD, check buttons, etc. here on each loop
  // Just be sure to feed the MP3 raw data at a BPS matching the recording mode
  // (i.e. a 128Kb MP3 only needs 128Kb (16K byte) of reads every second).  Don't disable
  // IRQs for long, either, as the audio DMA and calculations re driven off of them.
  //
  // If you don't send data fast enough (underflow) or you stop sending data completely,
  // the audio will silence and continue when more data is available.
}
