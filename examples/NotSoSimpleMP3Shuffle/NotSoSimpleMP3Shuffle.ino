/*
    NotSoSimpleMP3Shuffle

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

#include <BackgroundAudio.h>
#include <SD.h>
#include <vector>
#include <string.h>
#include "ID3.h"
#include "ID3Callback.h"

// This are GP pins for SPI0 on the Raspberry Pi Pico board, and connect
// to different *board* level pinouts.  Check the PCB while wiring.
// Only certain pins can be used by the SPI hardware, so if you change
// these be sure they are legal or the program will crash.
// See: https://datasheets.raspberrypi.com/picow/PicoW-A4-Pinout.pdf
const int _MISO = 4;  // AKA SPI RX
const int _MOSI = 7;  // AKA SPI TX
const int _CS = 5;
const int _SCK = 6;

#ifdef ESP32
#include <ESP32I2SAudio.h>
ESP32I2SAudio audio(4, 5, 6); // BCLK, LRCLK, DOUT (,MCLK)
#else
#include <PWMAudio.h>
#include <I2S.h>
// I2S audio(OUTPUT, 0, 2);
PWMAudio audio(0);
#endif
// We will make a larger buffer because SD cards can sometime take a long time to read
BackgroundAudioMP3Class<RawDataBuffer<16 * 1024>> BMP(audio);

// List of all MP3 files in the root directory
std::vector<String> mp3list;

// The file we're currently playing
File f;

// Read buffer that's better off not in the stack due to its size
uint8_t filebuff[512];

// Recursively scan the card and make a list of all MP3 files in all dirs
void scanDirectory(const char *dirname) {
  File root = SD.open(dirname);
  while (true) {
    f = root.openNextFile();
    if (!f) {
      break;
    }
    String n = f.name();
    n.toLowerCase();
    String path = dirname;
    path += f.name();
    if (f.isDirectory()) {
      if (f.name()[0] == '.') {
        // Ignore . and ..
        continue;
      }
      String sub = dirname;
      sub += f.name();
      sub += "/";
      scanDirectory(sub.c_str());
    } else if (strstr(n.c_str(), ".mp3")) {
      mp3list.push_back(path);
      Serial.printf("ADD: %s\n", path.c_str());
    } else {
      Serial.printf("SKP: %s\n", path.c_str());
    }
    f.close();
  }
  root.close();
}

void setup() {
  Serial.begin(115200);

  // Ensure a different random sequence every time we start up
#ifdef ESP32
  srand(ESP.getCycleCount());
#else
  srand(rp2040.hwrand32());
#endif

  // Initialize the SD card
  bool sdInitialized = false;
#ifdef ESP32
  SPI.begin(_SCK, _MISO, _MOSI, _CS);
  sdInitialized = SD.begin(_CS);
#else
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
#endif

  if (!sdInitialized) {
    Serial.println("initialization failed!");
    while (1) {
      delay(1000);
#ifdef ESP32
      ESP.restart();
#else
      rp2040.reboot();
#endif
    }
  }

  scanDirectory("/");

  // Start the background player
  BMP.begin();
}

void loop() {
  // When BOOTSEL held, skip to another song
#ifdef ESP32
  if (Serial.available()) {
    f.close();  // Choose another
    while (Serial.available()) {
      Serial.read();
    }
  }
#else
  if (BOOTSEL) {
    f.close();  // Choose another
    // wait for release
    while (BOOTSEL) {
      delay(1);
    }
  }
#endif

  // Choose a song from the list if there's no open file
  if (!f) {
    auto x = random(mp3list.size());
    f = SD.open(mp3list[x]);
    Serial.printf("\r\n\r\nNow playing: %s\r\n", mp3list[x].c_str());
    processID3(f, id3Callback);
    auto p = f.position();
    p = p & ~511; // Ensure on a sector boundary, MP3 will resync appropriately
    f.seek(p);
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
