/*
    BackgroundAudio
    Plays an audio file using IRQ driven decompression.  Main loop() writes
    data to the buffer but isn't blocked while playing

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
#include <stdint.h>

/**
    @brief Potentially scale output signal in-situ with saturation.

    @param [in, out] samples 16-bit stereo samples to be modified.  Will be overwritten by scaled output
    @param [in] count Number of 16-bit quantities to scale (i.e. 2x number oif stereo samples)
    @param [in] gain Fixed point 16.16 scale factor to apply to each sample
*/
static inline void ApplyGain(int16_t *samples, size_t count, int32_t gain) {
    // Default case, don't do anything
    if (gain == 1 << 16) {
        return;
    }

    if (gain > 1 << 16) { // May clip
        for (size_t i = 0; i < count; i++) {
            int32_t x = samples[i];
            x *= gain;
            x >>= 16;
            if (x < -32767) {
                x = -32767;
            } else if (x > 32767) {
                x = 32767;
            }
            samples[i] = x;
        }
    } else { // Can't clip, no need to check
        for (size_t i = 0; i < count; i++) {
            int32_t x = samples[i];
            x *= gain;
            x >>= 16;
            samples[i] = x;
        }
    }
}
