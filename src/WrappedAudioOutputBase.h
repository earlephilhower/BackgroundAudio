/*
    BackgroundAudio
    Plays an audio file using IRQ driven decompression.  Main loop() writes
    data to the buffer but isn't blocked while playing

    Copyright (c) 2025 Earle F. Philhower, III <earlephilhower@yahoo.com>

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

#ifdef ESP32

#include <Print.h>

/**
    @brief Abstract class for audio output devices to allow easy swapping between output devices.  Taken from Arduino-Pico
*/
class AudioOutputBase : public Print {
public:
    virtual ~AudioOutputBase() { }
    virtual bool setBuffers(size_t buffers, size_t bufferWords, int32_t silenceSample = 0) = 0;
    virtual bool setBitsPerSample(int bps) = 0;
    virtual bool setFrequency(int freq) = 0;
    virtual bool setStereo(bool stereo = true) = 0;
    virtual bool begin() = 0;
    virtual bool end() = 0;
    virtual bool getUnderflow() = 0;
    virtual void onTransmit(void(*)(void *), void *) = 0;
    // From Print
    virtual size_t write(const uint8_t *buffer, size_t size) = 0;
    virtual int availableForWrite() = 0;
};

#else

#include <AudioOutputBase.h>

#endif
