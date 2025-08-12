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
#include "BackgroundAudioSource.h"

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
class BackgroundAudioSpeechClass : public BackgroundAudioSource<DataBuffer, 1324, 5> {
public:
    BackgroundAudioSpeechClass() : BackgroundAudioSource<DataBuffer, 1324, 5>() {
        _voice = nullptr;
        this->_audio_frame = (int16_t*) malloc(sizeof(int16_t) * 1324 * 2); // 1324 samples, 2 channels
    }

    /**
        @brief Construct an output device using the specified physical audio output

        @param [in] d AudioOutputBase device to send data to (or AudioMixerInput)
    */
    BackgroundAudioSpeechClass(AudioOutputBase &d) : BackgroundAudioSource<DataBuffer, 1324, 5>(&d) {
        this->_audio_frame = (int16_t*) malloc(sizeof(int16_t) * 1324 * 2); // 1324 samples, 2 channels
    }

    ~BackgroundAudioSpeechClass() {
        free(this->_audio_frame);
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
        if (this->_playing || !_voice || !_voiceLen) {
            return false;
        }

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

        return BackgroundAudioSource<DataBuffer, 1324, 5>::begin();
    }

    /**
        @brief Speaks a C-String

        @param [in] string The string to speak.  Data is copied so this string can disappear afterwards
        @returns Number of bytes actually written to the buffer, or 0 on error (out of space)
    */
    size_t speak(const char *string) {
        if (this->availableForWrite() < strlen(string)) {
            return 0; // All or nothing
        }
        return this->write((const void *)string, strlen(string) + 1);
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
          @brief Determine if no more speech is present in the buffer

          @return True if no raw strings still left to process
    */
    bool done() override {
        return BackgroundAudioSource<DataBuffer, 1324, 5>::done() && !_generatingSpeech;
    }

    /**
            @brief Flushes any existing raw data, resets the processor to start a new speaking

            @details
            This is only needed to abort a currently speaking phrase.  Multiple strings can be sent
            in one after the other with `write` or `speak`
    */
    void flush() override {
        noInterrupts();
        this->_ib.flush();
        _generatingSpeech = false;
        short *mono;
        espeak_SynthesizeOneStep(&mono); // Thrown out
        espeak_AbortSynthesis();
        interrupts();
    }

private:
    static int _speechCB(short *data, int count, espeak_EVENT *events) {
        return 0; // Should never really be called by ESpeak internals
    }

    void generateOneFrame() {
        this->framelen = 0;

        // If we're not currently synthesizng speech, is there another string we can say?
        if (!_generatingSpeech) {
            if (this->_ib.available()) {
                const uint8_t *b = this->_ib.buffer();
                for (int i = 0; i < (int)this->_ib.available(); i++) {
                    if (!b[i]) {
                        espeak_Synth(this->_ib.buffer(), i, 0, (espeak_POSITION_TYPE)0, 0, espeakCHARS_AUTO, 0, this);
                        _generatingSpeech = true;
                        break;
                    }
                }
            }
        }

        if (_generatingSpeech && !this->framelen) {
            // Generate the raw samples
            short *mono;
            this->framelen = std::min(espeak_SynthesizeOneStep(&mono), (int)this->max_framelen);
            // Now convert to stereo by duplicating channels, store in frame buffer
            int16_t *ptr = this->_audio_frame;
            for (int i = 0; i < this->framelen; i++) {
                *ptr++ = *mono;
                *ptr++ = *mono++;
            }
            // Advance synthesis state and check if done
            if (!espeak_SynthesisGenerateNext()) {
                _generatingSpeech = false;
                this->_ib.shiftUp(strlen((const char *)this->_ib.buffer()) + 1); // Only shift out the speech once it's done speaking, easier to track
                this->_shifts++;
            }
        }
    }

private:
    bool _generatingSpeech = false;

    const unsigned char *_dict;
    size_t _dictLen;
    const unsigned char *_voice;
    size_t _voiceLen;
};


/**
    @brief General purpose speaker background player with an 8KB buffer.  Needs to have `write` called repeatedly with data.
*/
using BackgroundAudioSpeech = BackgroundAudioSpeechClass<RawDataBuffer<1024>>;