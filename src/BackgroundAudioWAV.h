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
#include <Arduino.h>
#include "WrappedAudioOutputBase.h"
#include "BackgroundAudioBuffers.h"
#include "BackgroundAudioGain.h"

// Interrupt-driven WAV decoder.  Generates a full frame of samples each cycle
// and uses the RawBuffer to safely hand data from the app to the decompressor.
template<class DataBuffer>
class BackgroundAudioWAVClass {
public:
    BackgroundAudioWAVClass() {
        _playing = false;
        _out = nullptr;
    }
    BackgroundAudioWAVClass(AudioOutputBase &d) {
        _playing = false;
        setDevice(&d);
    }
    ~BackgroundAudioWAVClass() {}

    bool setDevice(AudioOutputBase *d) {
        if (!_playing) {
            _out = d;
            return true;
        }
        return false;
    }

    void setGain(float scale) {
        _gain = (int32_t)(scale * (1 << 16));
    }

    bool begin() {
        if (!_out) {
            return false;
        }
        _seenRIFF = false;
        _seenFMT = false;
        _seenDATA = false;

        // We will use natural frame size to minimize mismatch
        _out->setBuffers(5, framelen);  // Framelen is in samples, but buffers takes words, which means 2 samples per word so we're good!
        _out->onTransmit(&_cb, (void *)this); // The pump we will use to generate our audio
        _out->setBitsPerSample(16);
        _out->setStereo(true);
        _out->setFrequency(44100);
        _out->begin();

        // Stuff with silence to start
        uint32_t zeros[32] = {};
        while (_out->availableForWrite() > 32) {
            _out->write((uint8_t *)zeros, sizeof(zeros));
        }
        _playing = true;

        return true;
    }

    void end() {
        _out->end();
    }

    bool playing() {
        return _playing;
    }

    size_t write(const void *data, size_t len) {
        return _ib.write((const uint8_t *)data, len);
    }

    size_t availableForWrite() {
        return _ib.availableForWrite();
    }

    size_t available() {
        return _ib.available() - _accumShift;
    }

    bool done() {
        return !available();
    }

    uint32_t frames() {
        return _frames;
    }

    uint32_t shifts() {
        return _shifts;
    }

    uint32_t underflows() {
        return _underflows;
    }

    uint32_t errors() {
        return _errors;
    }

    uint32_t dumps() {
        return _dumps;
    }

    void flush() {
        noInterrupts();
        _ib.flush();
        _seenRIFF = false;
        _seenFMT = false;
        _seenDATA = false;
        _dataSkipped = 0;
        _dataRemaining = 0;
        _accumShift = 0;
        interrupts();
    }

private:
    static void _cb(void *ptr) {
        ((BackgroundAudioWAVClass*)ptr)->pump();
    }

    void generateOneFrame() {
        int16_t *out = (int16_t *)_outSample;
        int16_t *end = &_outSample[framelen * 2];
        while (out < end) {
            // If we accumulate too large of a shift, actually do the shift so more space for writer
            if (_accumShift > _ib.size() / 2) {
                _ib.shiftUp(_accumShift);
                _accumShift = 0;
                _shifts++;
            }

            const unsigned char *b = _ib.buffer() + _accumShift;
            int avail = _ib.available() - _accumShift;
            if (avail <= 0) {
                goto underflow;
            }
            if (_dataSkipped) {
                int toSkip = std::min(_dataSkipped, avail);
                _accumShift += toSkip;
                _dataSkipped -= toSkip;
                continue;
            }
            if (_dataRemaining > 0) {
                if ((_channels == 1) && (_bps == 8)) {
                    if (avail < 1) {
                        goto underflow;
                    }
                    int16_t l = b[0]; // 0-255 unsigned
                    l -= 128; // -128...127 signed
                    l <<= 8; // -32K..32K signed
                    *out++ = l;
                    *out++ = l;
                    _accumShift++;
                } else if ((_channels == 2) && (_bps == 8)) {
                    if (avail < 2) {
                        goto underflow;
                    }
                    int16_t l = b[0]; // 0-255 unsigned
                    l -= 128; // -128...127 signed
                    l <<= 8; // -32K..32K signed
                    *out++ = l;
                    int16_t r = b[1]; // 0-255 unsigned
                    r -= 128; // -128...127 signed
                    r <<= 8; // -32K..32K signed
                    *out++ = r;
                    _accumShift += 2;
                } else if ((_channels == 1) && (_bps == 16)) {
                    if (avail < 2) {
                        goto underflow;
                    }
                    int16_t l = b[0] | (b[1] << 8);
                    *out++ = l;
                    *out++ = l;
                    _accumShift += 2;
                } else { // 16b, stereo
                    if (avail < 4) {
                        goto underflow;
                    }
                    int16_t l = b[0] | (b[1] << 8);
                    *out++ = l;
                    int16_t r = b[2] | (b[3] << 8);
                    *out++ = r;
                    _accumShift += 4;
                }
                _dataRemaining--;
                if (!_dataRemaining) {
                    _seenDATA = false;
                }
                continue;
            } else {
                if (avail < 4) { // Need 4 bytes to get a header type
                    goto underflow;
                }
                if (!memcmp(b, "RIFF", 4)) {
                    if (avail < 12) {
                        goto underflow;
                    }
                    if (!memcmp(b + 8, "WAVE", 4)) {
                        // This is a RIFF header.  We will ignore the total size
                        _seenRIFF = true;
                        _accumShift += 12; // Skip it
                        continue;
                    }
                    _accumShift++; // Error, shift along
                    continue;
                } else if (_seenRIFF && !_seenFMT && !memcmp(b, "fmt ", 4)) {
                    if (avail < 24) {
                        goto underflow;
                    }
                    if ((b[4] != 16) || b[5] || b[6] || b[7] || (b[8] != 1) || b[9]) {
                        // Length for format not PCM, ignore this
                        _errors++;
                        _seenRIFF = false;
                        _accumShift++;
                        continue;
                    }
                    _channels = b[10] | (b[11] << 8);
                    _sampleRate = b[12] | (b[13] << 8) | (b[14] << 16) | (b[15] << 24);
                    _bps = b[22] | (b[23] << 8);
                    if ((_channels == 0) || (_channels > 2) || !((_bps == 8) || (_bps == 16)) || (_sampleRate < 4000) || (_sampleRate > 48000)) {
                        // Invalid config, we can't play
                        _errors++;
                        _sampleRate = 0;
                        _seenRIFF = false;
                        _accumShift++;
                        continue;
                    }
                    _accumShift += 24;
                    _seenFMT = true;
                } else if (_seenRIFF && _seenFMT && !_seenDATA && !memcmp(b, "data", 4)) {
                    if (avail < 8) {
                        goto underflow;
                    }
                    _dataRemaining = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24); // in bytes
                    _dataRemaining /= _bps / 8;
                    _dataRemaining /= _channels; // Now in samples
                    _seenDATA = true;
                    _seenRIFF = false;
                    _seenFMT = false;
                    _accumShift += 8; // Jump to the data
                    continue;
                } else {
                    if (avail < 8) {
                        goto underflow;
                    }
                    _dataSkipped = b[4] | (b[5] << 8) | (b[6] << 16) | (b[7] << 24); // in bytes
                    _accumShift += 8;
                    // We'll just skip this chunk before processing
                    continue;
                }
            }
underflow:
            _underflows++;
            if (_accumShift) {
                _ib.shiftUp(_accumShift);
                _accumShift = 0;
                _shifts++;
            }
            // Because we're in IRQ and there's not enough data, we know no more can come it so just 0-fill remainder in tight loop
            while (out < end) {
                *out++ = 0;
                *out++ = 0;
            }
            continue;
        } // while(out < end-of-outsamples)

        ApplyGain(_outSample, framelen * 2, _gain);
    }



    void pump() {
        while (_out->availableForWrite() >= (int)framelen) {
            generateOneFrame();
            if (_sampleRate) {
                _out->setFrequency(_sampleRate);
            }
            _out->write((uint8_t *)_outSample, framelen * 2 * sizeof(int16_t));
        }
    }

private:
    AudioOutputBase *_out;
    bool _playing = false;
    static const size_t framelen = 512;
    DataBuffer _ib;
    int16_t _outSample[framelen * 2] __attribute__((aligned(4)));
    int32_t _gain = 1 << 16;
    uint32_t _accumShift = 0;
    bool _seenRIFF = false;
    bool _seenFMT = false;
    bool _seenDATA = false;
    int _dataRemaining = 0;
    int _dataSkipped = 0;

    int _sampleRate = 44100;
    int _channels = 2;
    int _bps = 16;

    // MP3 quality stats, cumulative
    uint32_t _frames = 0;
    uint32_t _shifts = 0;
    uint32_t _underflows = 0;
    uint32_t _errors = 0;
    uint32_t _dumps = 0;
};

using BackgroundAudioWAV = BackgroundAudioWAVClass<RawDataBuffer<8 * 1024>>;
using ROMBackgroundAudioWAV = BackgroundAudioWAVClass<ROMDataBuffer>;
