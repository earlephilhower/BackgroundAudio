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

/**
    @brief Interrupt-driven WAV decoder.  Generates a full frame of samples each cycle and uses the RawBuffer to safely hand data from the app to the decompressor.

    @tparam DataBuffer The type of raw data buffer, either normal `RawDataBuffer` or `ROMDataBuffer` as appropriate.
*/
template<class DataBuffer, size_t FrameLength, size_t BufferCount>
class BackgroundAudioSource {
public:
    BackgroundAudioSource() {
        _playing = false;
        _paused = false;
        _out = nullptr;
    }

    /**
        @brief Construct an AAC decoder with a given AudioOutputBase

        @param [in] d AudioOutputBase device (MixerInput or I2S or PWM, etc.) to decode tp
    */
    BackgroundAudioSource(AudioOutputBase &d) {
        _playing = false;
        _paused = false;
        setDevice(&d);
    }

    virtual ~BackgroundAudioSource() {}

    /**
        @brief Configure the output device before `begin`

        @return True if successful
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
        @brief Starts the background WAV decoder/player.  Will initialize the output device and start sending silence immediately.

        @return True on success, false if already started.
    */
    virtual bool begin() {
        if (!_out) {
            return false;
        }

        // We will use natural frame size to minimize mismatch
        _out->setBuffers(buffers, max_framelen);  // Framelen is in samples, but buffers takes words, which means 2 samples per word so we're good!
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

    /**
        @brief Stops the WAV decoder process and the calls the output device's end to shut it down, too.
    */
    void end() {
        if (_playing) {
            _out->end();
        }
        _playing = false;    }

    /**
        @brief Determines if the WAV decoder has been started

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
        @brief Determine if no more WAV file is present in the buffer

        @return True if no raw WAV data is still left to process
    */
    virtual bool done() {
        return !available();
    }

    /**
        @brief Get number of "frames" processed by decoder

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
        @brief Get the number of times the WAV decoder has underflowed waiting on raw data since `begin`

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
        @brief Flushes any existing WAV data, resets the processor to start a new WAV

        @details
        This is only needed if the current WAV file was corrupted or to stop playing one WAV
        file and immediately start on a new one.
    */
    virtual void flush() = 0;

    /**
        @brief Pause the decoder.  Won't process raw input data and will transmit silence
    */
    void pause() {
        _paused = true;
    }

    /**
        @brief Determine if the WAV playback is paused

        @return True of WAV playback has been paused
    */
    bool paused() {
        return _paused;
    }

    /**
        @brief Unpause previously paused WAV playback.  Will start processing input data again
    */
    void unpause() {
        _paused = false;
    }

private:
    static void _cb(void *ptr) {
        ((BackgroundAudioSource*)ptr)->pump();
    }

    virtual void generateOneFrame() = 0;



    void pump() {
        while (_out->availableForWrite() >= framelen) {
            if (_paused) {
                bzero(_audio_frame, framelen * 2 * sizeof(int16_t));
            } else {
                generateOneFrame();
                ApplyGain(_audio_frame, framelen * 2, _gain);
                if (_sampleRate) {
                    _out->setFrequency(_sampleRate);
                }
            }

            // Try to push the whole frame, but don't spin forever if we can't
            uint8_t* p = reinterpret_cast<uint8_t*>(_audio_frame);
            size_t remaining = framelen * 4;
            while (remaining) {
                size_t n = _out->write(p, remaining);
                p += n;
                remaining -= n;
            }
        }
    }

protected:
    AudioOutputBase *_out;
    bool _playing = false;
    bool _paused = false;
    static const size_t max_framelen = FrameLength;
    static const size_t buffers = BufferCount;
    size_t framelen = max_framelen;
    DataBuffer _ib;
    int16_t *_audio_frame;
    int32_t _gain = 1 << 16;
    uint32_t _accumShift = 0;
    int _sampleRate = 44100;

    // MP3 quality stats, cumulative
    uint32_t _frames = 0;
    uint32_t _shifts = 0;
    uint32_t _underflows = 0;
    uint32_t _errors = 0;
    uint32_t _dumps = 0;
};
