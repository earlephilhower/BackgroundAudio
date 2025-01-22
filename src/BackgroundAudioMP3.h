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
#include "BackgroundAudioGain.h"
#include "BackgroundAudioBuffers.h"
#include "libmad/config.h"
#include "libmad/mad.h"

// Interrupt-driven MP3 decoder.  Generates a full frame of samples each cycle
// and uses the RawBuffer to safely hand data from the app to the decompressor.
// Templated to avoid VTable indirection while supporting I2S and PWM.
template<class DataBuffer>
class BackgroundAudioMP3Class {
public:
    BackgroundAudioMP3Class() {
        _playing = false;
        _out = nullptr;
    }
    BackgroundAudioMP3Class(AudioOutputBase &d) {
        _playing = false;
        setDevice(&d);
    }
    ~BackgroundAudioMP3Class() {}

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
        // MP3 processing init
        mad_stream_init(&_stream);
        mad_frame_init(&_frame);
        mad_synth_init(&_synth);
        mad_stream_options(&_stream, 0);

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
        }
        _playing = false;
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
        return available() <= MAD_BUFFER_GUARD * 2; // At EOF we could bounce between n and 2n guard bytes when app checks
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
        _accumShift = 0;
        interrupts();
    }

private:
    static void _cb(void *ptr) {
        ((BackgroundAudioMP3Class*)ptr)->pump();
    }

    void generateOneFrame() {
        // Every frame requires shifting all remaining data (6K?) before processing.
        // We're not decoding MP3s, we're shifting data!  Instead, scroll down and only
        // shift when we're > 1/2 of the total buffer size.  We'll still shift to
        // allow new data to be written, but we'll do it much less frequently.

        // Shift out the used data
        if (_stream.buffer) {
            if (_stream.next_frame && _stream.next_frame != _stream.this_frame) {
                _accumShift += _stream.next_frame - _stream.this_frame;
            } else {
                // We have only a partial frame in the buffer.  We need data *now* do dump it and send zeros.  Underflow case
                _accumShift += maxFrameSize;
            }
        }

        // Try and find a frame header to send in.  Should be a single check in most cases since we should be aligned after the 1st frame
        int ptr = 0;
        int pend = _ib.available() - 1 - _accumShift;
        const uint8_t *b = _ib.buffer() + _accumShift;
        bool found = false;
        for (ptr = 0; ptr < pend; ptr++) {
            if ((b[ptr] == 0xff) && ((b[ptr + 1] & 0xe0) == 0xe0)) {
                _accumShift += ptr;
                found = true;
                break;
            }
        }
        if (!found) {
            // Couldn't find a header at all, the entire buffer is junk so dump it
            _accumShift = 0;
            _ib.shiftUp(_ib.available());
            _dumps++;
        }

        // If we accumulate too large of a shift, actually do the shift so more space for writer
        if (_accumShift > _ib.size() / 2) {
            _ib.shiftUp(_accumShift);
            _accumShift = 0;
            _shifts++;
        }

        // Ensure everything gets pumped out
        if (_ib.available() - _accumShift < MAD_BUFFER_GUARD) {
            _ib.write0(MAD_BUFFER_GUARD);
            _underflows++;
        }

        // Pass the new buffer information to libmad.
        mad_stream_buffer(&_stream, _ib.buffer() + _accumShift, _ib.available() - _accumShift);

        // Decode and send next frame
        if (mad_frame_decode(&_frame, &_stream)) {
            mad_frame_mute(&_frame);
            _errors++;
        }
        mad_synth_frame(&_synth, &_frame);
        _frames++;

        // Only stereo for sanity
        if (_synth.pcm.channels == 1) {
            for (size_t i = 0; i < framelen; i++) {
                _synth.pcm.samplesX[i][1] = _synth.pcm.samplesX[i][0];
            }
        }

        ApplyGain((int16_t*)_synth.pcm.samplesX, framelen * 2, _gain);
    }

    void pump() {
        while (_out->availableForWrite() >= (int)framelen) {
            generateOneFrame();
            if (_synth.pcm.samplerate) {
                _out->setFrequency(_synth.pcm.samplerate);
            }
            _out->write((uint8_t *)_synth.pcm.samplesX, _synth.pcm.length * 4);
        }
    }

private:
    AudioOutputBase *_out;
    bool _playing = false;
    static const size_t framelen = 1152;
    static const size_t maxFrameSize = 2881;
    DataBuffer _ib;
    struct mad_stream _stream;
    struct mad_frame _frame;
    struct mad_synth _synth;
    int32_t _gain = 1 << 16;
    uint32_t _accumShift = 0;

    // MP3 quality stats, cumulative
    uint32_t _frames = 0;
    uint32_t _shifts = 0;
    uint32_t _underflows = 0;
    uint32_t _errors = 0;
    uint32_t _dumps = 0;
};

using BackgroundAudioMP3 = BackgroundAudioMP3Class<RawDataBuffer<8 * 1024>>;
using ROMBackgroundAudioMP3 = BackgroundAudioMP3Class<ROMDataBuffer>;
