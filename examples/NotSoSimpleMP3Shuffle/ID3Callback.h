
/*
    NotSoSimpleMP3Shuffle - ID3 Callback

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

// In a real embedded application, only interesting fields would be verified
// and they would set global variables for the UI to update on the next loop()
// Here, just print to USB.
extern void id3Callback(int vers, char *field, int encoding, char *value, int valueLen);
