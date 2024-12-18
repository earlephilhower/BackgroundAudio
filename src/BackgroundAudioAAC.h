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
#include <AudioOutputBase.h>
#include "BackgroundAudioGain.h"
#include "BackgroundAudioBuffers.h"
#include "libhelix-aac/aacdec.h"


// Interrupt-driven AAC decoder.  Generates a full frame of samples each cycle
// and uses the RawBuffer to safely hand data from the app to the decompressor.
// Templated to avoid VTable indirection while supporting I2S and PWM.
template<class DataBuffer>
class BackgroundAudioAACClass {
public:
    BackgroundAudioAACClass(AudioOutputBase &d) {
        _playing = false;
        setDevice(d);
    }
    ~BackgroundAudioAACClass() {}

    bool setDevice(AudioOutputBase &d) {
        if (!_playing) {
            _out = &d;
            return true;
        }
        return false;
    }

    void setGain(float scale) {
        _gain = (int32_t)(scale * (1 << 16));
    }

    bool begin() {
        if (_playing || !_out) {
            return false;
        }

        _hAACDecoder = AACInitDecoder();
        if (!_hAACDecoder) {
            return false;
        }

        // We will use natural frame size to minimize mismatch
        _out->setBuffers(5, framelen);  // Framelen is in samples, but buffers takes words, which means 2 samples per word so we're good!
        _out->onTransmit(&_cb, (void *)this); // The pump we will use to generate our audio
        _out->setBitsPerSample(16);
        _out->setStereo(true);
        _out->setFrequency(44100);
        _out->begin();

        // Stuff with silence to start
        uint16_t zeros[32] __attribute__((aligned(4))) = {};
        while (_out->availableForWrite() > 32) {
            _out->write((uint8_t *)zeros, sizeof(zeros));
        }
        _playing = true;

        return true;
    }

    void end() {
        if (_playing) {
            _out->end();
            AACFreeDecoder(_hAACDecoder);
        }
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
        return available() <= 16; // Some minimum framesize
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
        _ib->flush();
        _accumShift = 0;
        interrupts();
    }

private:
    static void _cb(void *ptr) {
        ((BackgroundAudioAACClass*)ptr)->pump();
    }

    void generateOneFrame() {
        // Every frame requires shifting all remaining data (6K?) before processing.
        // We're not decoding AACs, we're shifting data!  Instead, scroll down and only
        // shift when we're > 1/2 of the total buffer size.  We'll still shift to
        // allow new data to be written, but we'll do it much less frequently.

        int nextFrame = AACFindSyncWord((uint8_t *)_ib.buffer() + _accumShift, _ib.available() - _accumShift);
        if (nextFrame == -1) {
            // Could not find a sync word but we need to send a frame now do dump entire buffer and play silence
            _ib.shiftUp(_ib.available());
            _accumShift = 0;
            bzero(_outSample, sizeof(_outSample));
            _errors++;
            _dumps++;
        } else {
            _accumShift += nextFrame;
            const unsigned char *inBuff = _ib.buffer() + _accumShift;
            int bytesLeft = _ib.available() - _accumShift;
            int ret = AACDecode(_hAACDecoder, (unsigned char **)&inBuff, &bytesLeft, (int16_t *)_outSample);
            if (ret) {
                // Error in decode, play silence and skip
                _accumShift++; // Just go one past the current bad sync and try again
                _errors++;
                bzero(_outSample, sizeof(_outSample));
            } else {
                AACFrameInfo fi;
                AACGetLastFrameInfo(_hAACDecoder, &fi);
                _sampleRate = fi.sampRateOut;
                _outSamples = fi.outputSamps / 2;
                _accumShift = inBuff - _ib.buffer();
                _frames++;
                if (fi.nChans == 1) {
                    for (size_t i = 0; i < _outSamples; i++) {
                        _outSample[i][1] = _outSample[1][0];
                    }
                }
            }
        }

        // If we accumulate too large of a shift, actually do the shift so more space for writer
        if (_accumShift > _ib.size() / 2) {
            _ib.shiftUp(_accumShift);
            _accumShift = 0;
            _shifts++;
        }

        ApplyGain((int16_t *)_outSample, _outSamples * 2, _gain);
    }

    void pump() {
        while (_out->availableForWrite() >= framelen) {
            generateOneFrame();
            if (_sampleRate) {
                _out->setFrequency(_sampleRate);
            }
            _out->write((uint8_t *)_outSample, _outSamples * 2 * sizeof(int16_t));
        }
    }

private:
    AudioOutputBase *_out = nullptr;
    HAACDecoder _hAACDecoder;
    bool _playing = false;
    static const size_t framelen = 2048;
    int16_t _outSample[framelen][2] __attribute__((aligned(4)));
    int _outSamples = 1024;
    int _sampleRate = 44000;
    DataBuffer _ib;
    int32_t _gain = 1 << 16;
    uint32_t _accumShift = 0;

    // MP3 quality stats, cumulative
    uint32_t _frames = 0;
    uint32_t _shifts = 0;
    uint32_t _underflows = 0;
    uint32_t _errors = 0;
    uint32_t _dumps = 0;
};

using BackgroundAudioAAC = BackgroundAudioAACClass<RawDataBuffer>;
using ROMBackgroundAudioAAC = BackgroundAudioAACClass<ROMDataBuffer>;
