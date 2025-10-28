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
#include "libespeak-ng/espeak-ng/speak_lib.h"
#include "libespeak-ng/phoneme/phonindex.h"
#include "libespeak-ng/phoneme/phontab.h"
#include "libespeak-ng/phoneme/phondata.h"
#include "libespeak-ng/phoneme/intonations.h"

// These will be defined by including a language
extern const unsigned char __espeakng_dict[];
extern size_t __espeakng_dictlen;

/**
    @brief Structure to collect a ESpeak-NG voice with its human-readable name
*/
typedef struct {
    /** Human-readable name */
    const char *name;
    /** Size of binary data */
    size_t len; // Size of binary data
    /** Binary data pointer in ROM/flash */
    const unsigned char *data;
} BackgroundAudioVoice;


/**
    @brief Interrupt-driven ESpeak-NG instance.  Generates a full frame of samples each cycle and uses the RawBuffer to safely hand data from the app to the decompressor.

    @tparam DataBuffer The type of raw data buffer, either normal `RawDataBuffer` or `ROMDataBuffer` as appropriate.
*/
template<class DataBuffer>
class BackgroundAudioSpeechClass {
public:
    BackgroundAudioSpeechClass() {
        _playing = false;
        _out = nullptr;
        _paused = false;
        _voice = nullptr;
    }

    /**
        @brief Construct an output device using the specified physical audio output

        @param [in] d AudioOutputBase device to send data to (or AudioMixerInput)
    */
    BackgroundAudioSpeechClass(AudioOutputBase &d) {
        _playing = false;
        _paused = false;
        setDevice(&d);
    }

    ~BackgroundAudioSpeechClass() {}

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
        @brief Sets the voice parameters (language customization)

        @param [in] v Voice (voice_xxx) included from in libespeak-ng/dict folder
    */
    void setVoice(BackgroundAudioVoice &v) {
        _voice = v.data;
        _voiceLen = v.len;
    }

    /**
        @brief Set the speaking rate in ~wpm, after calling begin()

        @param [in] rate WPM rate
    */
    void setRate(int rate) {
        espeak_SetParameter(espeakRATE, rate, 0);
    }

    /**
        @brief Adjust the pitch, 0...99, with 50 default.  After begin()

        @param [in] pitch Pitch setting
    */
    void setPitch(int pitch) {
        espeak_SetParameter(espeakPITCH, pitch, 0);
    }

    /**
        @brief Adjust the interword gap after begin()

        @param [in] gap Gap in ~10ms units at normal rate
    */
    void setWordGap(int gap) {
        espeak_SetParameter(espeakWORDGAP, gap, 0);
    }

    /**
        @brief Starts the background speaker.  Will initialize the output device and start sending silence immediately.

        @return True on success, false if already started.
    */
    bool begin() {
        if (_playing || !_voice || !_voiceLen || !_out) {
            return false;
        }

#ifdef ARDUINO_ARCH_RP2040
        _workIRQ = user_irq_claim_unused(true);
        _workObj = this;
        irq_set_exclusive_handler(_workIRQ, _irqStub);
        irq_set_priority(_workIRQ, 0xc0); // Lowest prio
        irq_set_enabled(_workIRQ, true);
#endif

        espeak_EnableSingleStep();
        espeak_InstallDict(__espeakng_dict, __espeakng_dictlen);
        espeak_InstallPhonIndex(_phonindex, sizeof(_phonindex));
        espeak_InstallPhonTab(_phontab, sizeof(_phontab));
        espeak_InstallPhonData(_phondata, sizeof(_phondata));
        espeak_InstallIntonations(_intonations, sizeof(_intonations));
        espeak_InstallVoice(_voice, _voiceLen);

        int samplerate = espeak_Initialize(AUDIO_OUTPUT_SYNCH_PLAYBACK, 20, nullptr, 0);
        espeak_SetVoiceByFile("INTERNAL");
        espeak_SetSynthCallback(_speechCB);

        // We will use natural frame size to minimize mismatch
        _out->setBuffers(5, framelen);
        _out->onTransmit(&_cb, (void *)this); // The pump we will use to generate our audio
        _out->setBitsPerSample(16);
        _out->setStereo(true);
        _out->setFrequency(samplerate);
        _out->begin();

        // Stuff with silence to start
        uint16_t zeros[32] __attribute__((aligned(4))) = {};
        while (_out->availableForWrite() > 32 * 2) {
            _out->write((uint8_t *)zeros, sizeof(zeros));
        }

        _playing = true;

        return true;
    }

    /**
          @brief Stops the process and the calls the output device's end to shut it down, too.
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
        @brief Determines if the speaker has been started

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
        @brief Speaks a C-String

        @param [in] string The string to speak.  Data is copied so this string can disappear afterwards
        @returns Number of bytes actually written to the buffer, or 0 on error (out of space)
    */
    size_t speak(const char *string) {
        if (availableForWrite() < strlen(string)) {
            return 0; // All or nothing
        }
        return write((const void *)string, strlen(string) + 1);
    }

    /**
        @brief Speaks an Arduino String

        @param [in] string The string to speak.  Data is copied so this string can disappear afterwards
        @returns Number of bytes actually written to the buffer, or 0 on error (out of space)
    */
    size_t speak(const String &string) {
        return speak(string.c_str());
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
        return _ib.available();
    }

    /**
          @brief Determine if no more speech is present in the buffer

          @return True if no raw strings still left to process
    */
    bool done() {
        return !available() && !_generatingSpeech;
    }

    /**
          @brief Get number of "frames" processed by speaker

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
           @brief Get the number of times the speaker has underflowed waiting on raw data since `begin`

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
            @brief Flushes any existing raw data, resets the processor to start a new speaking

            @details
            This is only needed to abort a currently speaking phrase.  Multiple strings can be sent
            in one after the other with `write` or `speak`
    */
    void flush() {
        noInterrupts();
        _ib.flush();
        _generatingSpeech = false;
        short *mono;
        espeak_SynthesizeOneStep(&mono); // Thrown out
        espeak_AbortSynthesis();
        interrupts();
    }

private:
#ifdef ARDUINO_ARCH_RP2040
    static void _irqStub() {
        BackgroundAudioSpeechClass<DataBuffer>::_workObj->pump();
    }

    static void _cb(void *ptr) {
        // Don't actually do work in the DMA interrupt, do it in the work IRQ context (low prio)
        irq_set_pending(BackgroundAudioSpeechClass<DataBuffer>::_workIRQ);
    }
#else
    static void _cb(void *ptr) {
        ((BackgroundAudioSpeechClass*)ptr)->pump();
    }
#endif

    static int _speechCB(short *data, int count, espeak_EVENT *events) {
        return 0; // Should never really be called by ESpeak internals
    }

    void generateOneFrame() {
        _frameLen = 0;

        // If we're not currently synthesizng speech, is there another string we can say?
        if (!_generatingSpeech) {
            if (_ib.available()) {
                const uint8_t *b = _ib.buffer();
                for (int i = 0; i < (int)_ib.available(); i++) {
                    if (!b[i]) {
                        espeak_Synth(_ib.buffer(), i, 0, (espeak_POSITION_TYPE)0, 0, espeakCHARS_AUTO, 0, this);
                        _generatingSpeech = true;
                        break;
                    }
                }
            }
        }

        if (_generatingSpeech && !_frameLen) {
            // Generate the raw samples
            short *mono;
            _frameLen = std::min(espeak_SynthesizeOneStep(&mono), framelen);
            // Now convert to stereo by duplicating channels, store in frame buffer
            int16_t *ptr = _frame;
            for (int i = 0; i < _frameLen; i++) {
                *ptr++ = *mono;
                *ptr++ = *mono++;
            }
            // Amplify if requested
            ApplyGain(_frame, _frameLen * 2, _gain);
            // Advance synthesis state and check if done
            if (!espeak_SynthesisGenerateNext()) {
                _generatingSpeech = false;
                _ib.shiftUp(strlen((const char *)_ib.buffer()) + 1); // Only shift out the speech once it's done speaking, easier to track
                _shifts++;
            }
        }
    }

#ifdef ARDUINO_ARCH_RP2040
public:
#endif
    void pump() {
        while (_out->availableForWrite() >= (int)(framelen * 4)) {
            if (!_frameLen && !_paused) {
                generateOneFrame();
            }
            if (_paused || !_frameLen) {
                bzero(_frame, sizeof(_frame));
                assert(_out->write((uint8_t *)_frame, sizeof(_frame)) == sizeof(_frame));
            } else {
                assert(_out->write((uint8_t *)_frame, _frameLen * 4) == (size_t)(_frameLen * 4));
                _frameLen = 0;
            }
        }
#ifdef ARDUINO_ARCH_RP2040
        irq_clear(_workIRQ);
#endif
    }

#ifdef ARDUINO_ARCH_RP2040
    static uint8_t _workIRQ;
    static BackgroundAudioSpeechClass<DataBuffer> *_workObj;
#endif

private:
    AudioOutputBase *_out;
    bool _playing = false;
    bool _paused = false;
    DataBuffer _ib;
    int32_t _gain = 1 << 16;
    bool _generatingSpeech = false;
    static constexpr int framelen = 1324; // From the 22050 normal samplerate and 20 length
    int16_t _frame[framelen * 2]; // Overprovision in case we get a long speech frame
    int _frameLen = 0;

    const unsigned char *_dict;
    size_t _dictLen;
    const unsigned char *_voice;
    size_t _voiceLen;

    // Quality stats, cumulative
    uint32_t _frames = 0;
    uint32_t _shifts = 0;
    uint32_t _underflows = 0;
    uint32_t _errors = 0;
    uint32_t _dumps = 0;
};

#ifdef ARDUINO_ARCH_RP2040
template<class DataBuffer> uint8_t BackgroundAudioSpeechClass<DataBuffer>::_workIRQ;
template<class DataBuffer> BackgroundAudioSpeechClass<DataBuffer> *BackgroundAudioSpeechClass<DataBuffer>::_workObj;
#endif

/**
    @brief General purpose speaker background player with an 8KB buffer.  Needs to have `write` called repeatedly with data.
*/
using BackgroundAudioSpeech = BackgroundAudioSpeechClass<RawDataBuffer<1024>>;
