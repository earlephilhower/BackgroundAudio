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

/**
    @brief Interrupt-driven MP3 decoder.  Generates a full frame of samples each cycle and uses the RawBuffer to safely hand data from the app to the decompressor.

    @tparam DataBuffer The type of raw data buffer, either normal `RawDataBuffer` or `ROMDataBuffer` as appropriate.
*/
template<class DataBuffer>
class BackgroundAudioMP3Class {
public:
    BackgroundAudioMP3Class() {
        _playing = false;
        _out = nullptr;
        _paused = false;
    }

    /**
        @brief Construct an MP3 output device using the specified physical audio output

        @param [in] d AudioOutputBase device to send data to (or AudioMixerInput)
    */
    BackgroundAudioMP3Class(AudioOutputBase &d) {
        _playing = false;
        _paused = false;
        setDevice(&d);
    }

    ~BackgroundAudioMP3Class() {}

    /**
        @brief Set an output device before `begin`

        @param [in] d Pointer to `AudioOutputDevice` to send decoded data to

        @return True on success
    */
    bool setDevice(AudioOutputBase *d) {
        if (!_playing) {
            _out = d;
            return true;
        }
        return false;
    }

    /**
            @brief Set the gain multiplier (volume) for the stream.  Takes effect immediately.

            @param [in] scale Floating point value from 0.0....16.0 to multiply all audio data by
    */
    void setGain(float scale) {
        _gain = (int32_t)(scale * (1 << 16));
    }




    /**
        @brief Starts the background MP3 decoder/player.  Will initialize the output device and start sending silence immediately.

        @return True on success, false if already started.
    */
    bool begin() {
        if (_playing || !_out) {
            return false;
        }

#ifdef ARDUINO_ARCH_RP2040
        _workIRQ = user_irq_claim_unused(true);
        _workObj = this;
        irq_set_exclusive_handler(_workIRQ, _irqStub);
        irq_set_priority(_workIRQ, 0xc0); // Lowest prio
        irq_set_enabled(_workIRQ, true);
#endif

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

    /**
          @brief Stops the MP3 decoder process and the calls the output device's end to shut it down, too.
    */
    void end() {
        if (_playing) {
#ifdef ARDUINO_ARCH_RP2040
            irq_set_enabled(_workIRQ, false);
            user_irq_unclaim(_workIRQ);
#endif
            _out->end();
        }
        _playing = false;
    }

    /**
        @brief Determines if the MP3 decoder has been started

        @returns True if running
    */
    bool playing() {
        return _playing;
    }

    /**
           @brief Writes a block of raw data to the decoder's buffer

           @details
           Copies up to `len` bytes into the raw buffer for the object.  Can be called before `begin`,
           and can write less fewer than the number of bytes requested if there is not enough space.
           Will not block.

           For ROM buffers this does not actually copy data, only the pointer.  Therefore, for ROM
           buffers you cannot concatenate data by calling multiple writes because the second write
           will throw out all the data from the first one.  Use `flush` and `write` when ready for
           a completely new buffer.

           @param [in] data Uncompressed input data
           @param [in] len Number of bytes to write

           @return Number of bytes written
    */
    size_t write(const void *data, size_t len) {
        return _ib.write((const uint8_t *)data, len);
    }

    /**
           @brief Gets number of bytes available to write to raw buffer

           @details
           For ROM buffers, this is always 0 after a write has happened.  Because ROM buffers don't
           actually allocate any RAM for buffering and simply store the pointer passed in from `write`
           there is no real space available for an app to `write` to.  An app can simply `flush` and
           `write` a new complete buffer if needed ignoring `availableForWrite` in the ROM case.

           @return Bytes that can be written
    */
    size_t availableForWrite() {
        return _ib.availableForWrite();
    }

    /**
           @brief Gets number of bytes already in the raw buffer

           @return Bytes of raw data in the buffer
    */
    size_t available() {
        return _ib.available() - _accumShift;
    }

    /**
          @brief Determine if no more MP3 file is present in the buffer

          @return True if no raw MP3 data is still left to process
    */
    bool done() {
        return available() <= MAD_BUFFER_GUARD * 2; // At EOF we could bounce between n and 2n guard bytes when app checks
    }

    /**
          @brief Get number of "frames" (1152 stereo samples) processed by decoder

          @return Number of frames, where frames are `framelen` stereo samples in size
    */
    uint32_t frames() {
        return _frames;
    }

    /**
            @brief Get the number of input data shifts processed by decoder since `begin`

            @return Number of times data has been shifted in the raw input buffer
    */
    uint32_t shifts() {
        return _shifts;
    }

    /**
           @brief Get the number of times the MP3 decoder has underflowed waiting on raw data since `begin`

           @return Number of frames of underflow data have occurred
    */
    uint32_t underflows() {
        return _underflows;
    }

    /**
          @brief Get the number of decoder errors since `begin`

          @return Number of decode errors encountered
    */
    uint32_t errors() {
        return _errors;
    }

    /**
        @brief Get the number of full buffer dumps (catastrophic data error) since `begin`

        @return Number of complete buffer throw-aways
    */
    uint32_t dumps() {
        return _dumps;
    }

    /**
           @brief Pause the decoder.  Won't process raw input data and will transmit silence
    */
    void pause() {
        _paused = true;
    }

    /**
        @brief Determine if the playback is paused

        @return True of WAV playback has been paused
    */
    bool paused() {
        return _paused;
    }


    /**
        @brief Unpause previously paused playback.  Will start processing input data again
    */
    void unpause() {
        _paused = false;
    }

    /**
            @brief Flushes any existing raw data, resets the processor to start a new MP3

            @details
            This is only needed to abort a currently playing MP3 file (i.e. skipping a track in the middle).
            Multiple MP3 files can just be concatenated together in the input buffer with `write`
    */
    void flush() {
        noInterrupts();
        _ib.flush();
        _accumShift = 0;
        interrupts();
    }

private:
#ifdef ARDUINO_ARCH_RP2040
    static void _irqStub() {
        BackgroundAudioMP3Class<DataBuffer>::_workObj->pump();
    }

    static void _cb(void *ptr) {
        // Don't actually do work in the DMA interrupt, do it in the work IRQ context (low prio)
        irq_set_pending(BackgroundAudioMP3Class<DataBuffer>::_workIRQ);
    }
#else
    static void _cb(void *ptr) {
        ((BackgroundAudioMP3Class*)ptr)->pump();
    }
#endif

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

#ifdef ARDUINO_ARCH_RP2040
public:
#endif
    void pump() {
        while (_out->availableForWrite() >= (int)(framelen * 4)) {
            if (_paused) {
                bzero(_synth.pcm.samplesX, _synth.pcm.length * 4);
            } else {
                generateOneFrame();
                if (_synth.pcm.samplerate) {
                    _out->setFrequency(_synth.pcm.samplerate);
                }
            }
            assert(_out->write((uint8_t *)_synth.pcm.samplesX, _synth.pcm.length * 4) == _synth.pcm.length * 4);
        }
#ifdef ARDUINO_ARCH_RP2040
        irq_clear(_workIRQ);
#endif
    }

#ifdef ARDUINO_ARCH_RP2040
    static uint8_t _workIRQ;
    static BackgroundAudioMP3Class<DataBuffer> *_workObj;
#endif

private:
    AudioOutputBase *_out;
    bool _playing = false;
    bool _paused = false;

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

#ifdef ARDUINO_ARCH_RP2040
template<class DataBuffer> uint8_t BackgroundAudioMP3Class<DataBuffer>::_workIRQ;
template<class DataBuffer> BackgroundAudioMP3Class<DataBuffer> *BackgroundAudioMP3Class<DataBuffer>::_workObj;
#endif

/**
    @brief General purpose MP3 background player with an 8KB buffer.  Needs to have `write` called repeatedly with data.
*/
using BackgroundAudioMP3 = BackgroundAudioMP3Class<RawDataBuffer<8 * 1024>>;

/**
    @brief Special purpose MP3 player for use with ROM or data already completely in RAM. Does not copy any data, uses single written data pointer directly.
*/
using ROMBackgroundAudioMP3 = BackgroundAudioMP3Class<ROMDataBuffer>;
