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
#include "libhelix-aac/aacdec.h"

/**
    @brief Interrupt-driven AAC decoder.  Generates a full frame of samples each cycle  and uses the RawBuffer to safely hand data from the app to the decompressor.

    @tparam DataBuffer The type of raw data buffer, either normal `RawDataBuffer` or `ROMDataBuffer` as appropriate.
*/
template<class DataBuffer>
class BackgroundAudioAACClass {
public:
    BackgroundAudioAACClass() {
        _playing = false;
        _paused = false;
        _out = nullptr;
    }

    /**
        @brief Construct an AAC decoder with a given AudioOutputBase

        @param [in] d AudioOutputBase device (MixerInput or I2S or PWM, etc.) to decode tp
    */
    BackgroundAudioAACClass(AudioOutputBase &d) {
        _playing = false;
        _paused = false;
        setDevice(&d);
    }

    ~BackgroundAudioAACClass() {}

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
          @brief Starts the background AAC decoder/player.  Will initialize the output device and start sending silence immediately.

          @details
          Note that this needs to allocate a significant large and contiguout amount of heap to function,
          so it should be called early on in the program to ensure that allocation can succeed.

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

        _hAACDecoder = AACInitDecoderPre(_private, sizeof(_private));
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

    /**
          @brief Stops the AAC decoder process and the calls the output device's end to shut it down, too.
    */
    void end() {
        if (_playing) {
#ifdef ARDUINO_ARCH_RP2040
            irq_set_enabled(_workIRQ, false);
            user_irq_unclaim(_workIRQ);
#endif
            _out->end();
        }
    }
    /**
         @brief Determines if the AAC decoder has been started

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
             @brief Determine if no more AAC file is present in the buffer

             @return True if no raw AAC data is still left to process
    */
    bool done() {
        return available() <= 16; // Some minimum framesize
    }

    /**
             @brief Get number of "frames" (1024 or 2048 stereo samples) processed by decoder

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
              @brief Get the number of times the AAC decoder has underflowed waiting on raw data since `begin`

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
                @brief Flushes any existing raw data, resets the processor to start a new AAC

                @details
                This is only needed to abort a currently playing AAC file (i.e. skipping a track in the middle).
                Multiple AAC files can just be concatenated together in the input buffer with `write`
    */
    void flush() {
        noInterrupts();
        _ib.flush();
        _accumShift = 0;
        interrupts();
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

private:
#ifdef ARDUINO_ARCH_RP2040
    static void _irqStub() {
        BackgroundAudioAACClass<DataBuffer>::_workObj->pump();
    }

    static void _cb(void *ptr) {
        // Don't actually do work in the DMA interrupt, do it in the work IRQ context (low prio)
        irq_set_pending(BackgroundAudioAACClass<DataBuffer>::_workIRQ);
    }
#else
    static void _cb(void *ptr) {
        ((BackgroundAudioAACClass*)ptr)->pump();
    }
#endif

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
                    for (int i = 0; i < _outSamples; i++) {
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

#ifdef ARDUINO_ARCH_RP2040
public:
#endif
    void pump() {
        while (_out->availableForWrite() >= (int)(framelen * 2 * sizeof(int16_t))) {
            if (_paused) {
                bzero((uint8_t *)_outSample, _outSamples * 2 * sizeof(int16_t));
            } else {
                generateOneFrame();
                if (_sampleRate) {
                    _out->setFrequency(_sampleRate);
                }
            }
            assert(_out->write((uint8_t *)_outSample, _outSamples * 2 * sizeof(int16_t)) == _outSamples * 2 * sizeof(int16_t));
        }
#ifdef ARDUINO_ARCH_RP2040
        irq_clear(_workIRQ);
#endif
    }

#ifdef ARDUINO_ARCH_RP2040
    static uint8_t _workIRQ;
    static BackgroundAudioAACClass<DataBuffer> *_workObj;
#endif

private:
    AudioOutputBase *_out = nullptr;
    HAACDecoder _hAACDecoder;
    uint8_t _private[/*sizeof(AACDecInfo)*/ 96 + /*sizeof(PSInfoBase)*/ 28752 + /*sizeof(PSInfoSBR)*/ 50788 + 16];
    bool _playing = false;
    bool _paused = false;
    static const size_t framelen = 2048;
    int16_t _outSample[framelen][2] __attribute__((aligned(4)));
    int _outSamples = 1024;
    int _sampleRate = 44000;
    DataBuffer _ib;
    int32_t _gain = 1 << 16;
    uint32_t _accumShift = 0;

    // AAC quality stats, cumulative
    uint32_t _frames = 0;
    uint32_t _shifts = 0;
    uint32_t _underflows = 0;
    uint32_t _errors = 0;
    uint32_t _dumps = 0;
};

#ifdef ARDUINO_ARCH_RP2040
template<class DataBuffer> uint8_t BackgroundAudioAACClass<DataBuffer>::_workIRQ;
template<class DataBuffer> BackgroundAudioAACClass<DataBuffer> *BackgroundAudioAACClass<DataBuffer>::_workObj;
#endif

/**
    @brief General purpose AAC background player with an 8KB buffer.  Needs to have `write` called repeatedly with data.
*/
using BackgroundAudioAAC = BackgroundAudioAACClass<RawDataBuffer<8 * 1024>>;

/**
    @brief Special purpose AAC player for use with ROM or data already completely in RAM. Does not copy any data, uses single written data pointer directly.
*/using ROMBackgroundAudioAAC = BackgroundAudioAACClass<ROMDataBuffer>;
