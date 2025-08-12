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
#include "BackgroundAudioSource.h"
#include "libmad/config.h"
#include "libmad/mad.h"

/**
    @brief Interrupt-driven MP3 decoder.  Generates a full frame of samples each cycle and uses the RawBuffer to safely hand data from the app to the decompressor.

    @tparam DataBuffer The type of raw data buffer, either normal `RawDataBuffer` or `ROMDataBuffer` as appropriate.
*/
template<class DataBuffer>
class BackgroundAudioMP3Class : public BackgroundAudioSource<DataBuffer, 1152, 5> {
public:
    BackgroundAudioMP3Class() : BackgroundAudioSource<DataBuffer, 1152, 5>() {
    }

    /**
        @brief Construct an MP3 output device using the specified physical audio output

        @param [in] d AudioOutputBase device to send data to (or AudioMixerInput)
    */
    BackgroundAudioMP3Class(AudioOutputBase &d) : BackgroundAudioSource<DataBuffer, 1152, 5>(&d) {
    }

    ~BackgroundAudioMP3Class() {}


    /**
        @brief Starts the background MP3 decoder/player.  Will initialize the output device and start sending silence immediately.

        @return True on success, false if already started.
    */
    bool begin() override {
        // MP3 processing init
        mad_stream_init(&_stream);
        mad_frame_init(&_frame);
        mad_synth_init(&_synth);
        mad_stream_options(&_stream, 0);

        // We will use natural frame size to minimize mismatch
        return BackgroundAudioSource<DataBuffer, 1152, 5>::begin();
    }

    /**
          @brief Determine if no more MP3 file is present in the buffer

          @return True if no raw MP3 data is still left to process
    */
    bool done() override {
        return this->available() <= MAD_BUFFER_GUARD * 2; // At EOF we could bounce between n and 2n guard bytes when app checks
    }

    /**
            @brief Flushes any existing raw data, resets the processor to start a new MP3

            @details
            This is only needed to abort a currently playing MP3 file (i.e. skipping a track in the middle).
            Multiple MP3 files can just be concatenated together in the input buffer with `write`
    */
    void flush() override {
        noInterrupts();
        this->_ib.flush();
        this->_accumShift = 0;
        interrupts();
    }

private:

    void generateOneFrame() override {
        // Every frame requires shifting all remaining data (6K?) before processing.
        // We're not decoding MP3s, we're shifting data!  Instead, scroll down and only
        // shift when we're > 1/2 of the total buffer size.  We'll still shift to
        // allow new data to be written, but we'll do it much less frequently.

        // Shift out the used data
        if (_stream.buffer) {
            if (_stream.next_frame && _stream.next_frame != _stream.this_frame) {
                this->_accumShift += _stream.next_frame - _stream.this_frame;
            } else {
                // We have only a partial frame in the buffer.  We need data *now* do dump it and send zeros.  Underflow case
                this->_accumShift += maxFrameSize;
            }
        }

        // Try and find a frame header to send in.  Should be a single check in most cases since we should be aligned after the 1st frame
        int ptr = 0;
        int pend = this->_ib.available() - 1 - this->_accumShift;
        const uint8_t *b = this->_ib.buffer() + this->_accumShift;
        bool found = false;
        for (ptr = 0; ptr < pend; ptr++) {
            if ((b[ptr] == 0xff) && ((b[ptr + 1] & 0xe0) == 0xe0)) {
                this->_accumShift += ptr;
                found = true;
                break;
            }
        }
        if (!found) {
            // Couldn't find a header at all, the entire buffer is junk so dump it
            this->_accumShift = 0;
            this->_ib.shiftUp(this->_ib.available());
            this->_dumps++;
        }

        // If we accumulate too large of a shift, actually do the shift so more space for writer
        if (this->_accumShift > this->_ib.size() / 2) {
            this->_ib.shiftUp(this->_accumShift);
            this->_accumShift = 0;
            this->_shifts++;
        }

        // Ensure everything gets pumped out
        if (this->_ib.available() - this->_accumShift < MAD_BUFFER_GUARD) {
            this->_ib.write0(MAD_BUFFER_GUARD);
            this->_underflows++;
        }

        // Pass the new buffer information to libmad.
        mad_stream_buffer(&_stream, this->_ib.buffer() + this->_accumShift, this->_ib.available() - this->_accumShift);

        // Decode and send next frame
        if (mad_frame_decode(&_frame, &_stream)) {
            mad_frame_mute(&_frame);
            this->_errors++;
        }
        mad_synth_frame(&_synth, &_frame);
        this->_frames++;

        // Only stereo for sanity
        if (_synth.pcm.channels == 1) {
            for (size_t i = 0; i < this->framelen; i++) {
                _synth.pcm.samplesX[i][1] = _synth.pcm.samplesX[i][0];
            }
        }
        this->_audio_frame = reinterpret_cast<int16_t*>(_synth.pcm.samplesX);
    }

private:
    struct mad_stream _stream;
    struct mad_frame _frame;
    struct mad_synth _synth;
    static const size_t maxFrameSize = 2881;
};


/**
    @brief General purpose MP3 background player with an 8KB buffer.  Needs to have `write` called repeatedly with data.
*/
using BackgroundAudioMP3 = BackgroundAudioMP3Class<RawDataBuffer<8 * 1024>>;

/**
    @brief Special purpose MP3 player for use with ROM or data already completely in RAM. Does not copy any data, uses single written data pointer directly.
*/
using ROMBackgroundAudioMP3 = BackgroundAudioMP3Class<ROMDataBuffer>;
