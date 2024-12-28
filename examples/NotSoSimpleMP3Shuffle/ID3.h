/*
    NotSoSimpleMP3Shuffle - ID3 Processing

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

#pragma once

#include <Arduino.h>
#include <FS.h>

// Check if there is an ID3 header and decode frames with a callback
extern void processID3(File f, void(*cb)(int vers, char *field, int encoding, char *value, int valueLen));

// This lets us do a uint32 compare and not a strcmp because we know we have at least 4 value values (xxx0 or xxxx0) in the field name
static inline constexpr uint32_t M(const char *s) {
  return s[0] | (s[1] << 8) | (s[2] << 16) | (s[3] << 24);
}

// Convert a potentially UTF-16 encoded string to a UTF-8 one for easier display.  Only 1 shared buffer so copy out before calling again!
const char *toUTF8(int encoding, const char *str, int strBytes);

// strlen for utf-16 without alignment restriction of std library
int myu16strlen(const char *ptr);
