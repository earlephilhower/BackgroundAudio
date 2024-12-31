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
#include <vector>

// The mixer input will buffer output from one audio source and resample to
// the mixer frequency so it can be summed up and output.

// Each input will have a buffer ready to be summed with the others.  The
// mixer will take the pointers to this and do the sum operation w/o actually
// copying it out.  As data is read from the mixerInputBuffer, it will be
// repopulated with (resampled) data from the source buffers.  Every time the
// a full source buffer is read a CB will be fired, just like in the
// AudioBufferManager.  (i.e. the mixedrinput will behave like the ABM for
// simplicity.)

class BackgroundAudioMixerInput : public AudioOutputBase {
private:
    // Forward definitions
    template<size_t> friend class BackgroundAudioMixer;
    struct AudioBuffer;

public:
    BackgroundAudioMixerInput(int outputRate, size_t outputBufferWords) {
        _outputRate = outputRate;
        _outputBufferWords = outputBufferWords;
        _outputBuffer = new uint32_t[outputBufferWords];
        _bufferCount = 5;
        _wordsPerBuffer = 2048;
        _silenceSample = 0;
        _inputRate = outputRate;
        _running = false;
        _cb = nullptr;
    }

    virtual ~BackgroundAudioMixerInput() {
        delete[] _outputBuffer;
        while (_filled) {
            auto x = _filled->next;
            _deleteAudioBuffer(_filled);
            _filled = x;
        }
        while (_empty) {
            auto x = _empty->next;
            _deleteAudioBuffer(_empty);
            _empty = x;
        }
    }

    virtual bool setBuffers(size_t buffers, size_t bufferWords, int32_t silenceSample = 0) override {
        _bufferCount = buffers;
        _wordsPerBuffer = bufferWords;
        _silenceSample = silenceSample;
        return true;
    }

    virtual bool setBitsPerSample(int bps) override {
        return bps == 16;  // Only 16b for now
    }

    virtual bool setFrequency(int freq) override {
        if ((int)_inputRate != freq) {
            _readOff = 0;
        }

        _inputRate = freq;
        // 2-step to ensure we do the math and not short-circuit
        _resample16p16 = _inputRate << 16;
        _resample16p16 /= _outputRate;
        return true;
    }

    virtual bool setStereo(bool stereo = true) override {
        return stereo;  // Only stereo for now
    }

    virtual bool begin() override {
        if (_running) {
            return false;
        }

        // The raw input data
        _empty = nullptr;
        for (size_t i = 0; i < _bufferCount; i++) {
            auto ab = new AudioBuffer;
            ab->buff = new uint32_t[_wordsPerBuffer];
            bzero(ab->buff, _wordsPerBuffer * 4);
            ab->next = nullptr;
            _addToList(&_empty, ab);
        }
        _filled = nullptr;
        _userOff = 0;
        _underflow = false;

        // The offset in the filled buffer to use next for sending to mixer
        _readOff = 0;
        setFrequency(_outputRate);

        _running = true;

        return true;
    }

    virtual bool end() override {
        return false;
    }

    virtual bool getUnderflow() override {
        noInterrupts();
        auto ret = _underflow;
        _underflow = false;
        interrupts();
        return ret;
    }

    virtual void onTransmit(void (*cb)(void *), void *cbData) override {
        _cb = cb;
        _cbData = cbData;
    }

    // From Print
    virtual size_t write(uint8_t) {
        return 0;  // Can't do bytes, dude!
    }

    virtual size_t write(const uint8_t *buffer, size_t size) override {
        size_t written = 0;

        if (!_running) {
            return 0;
        }
        size_t words = size / sizeof(uint32_t);
        while (words) {
            AudioBuffer **volatile p = (AudioBuffer * *volatile)&_empty;
            if (!*p) {
                break;
            }
            size_t availToWriteThisBuff = _wordsPerBuffer - _userOff;
            size_t toWrite = std::min(availToWriteThisBuff, words);
            memcpy(&((*p)->buff[_userOff]), buffer, toWrite * sizeof(uint32_t));
            buffer += toWrite * sizeof(uint32_t);
            written += toWrite;
            _userOff += toWrite;
            words -= toWrite;
            if (_userOff == _wordsPerBuffer) {
                _addToList(&_filled, _takeFromList(p));
                _userOff = 0;
            }
        }
        return written * sizeof(uint32_t);
    }

    virtual int availableForWrite() override {
        if (!_empty) {
            return 0;
        }
        AudioBuffer *p = _empty;
        int avail = _wordsPerBuffer - _userOff;  // Currently available in this buffer

        // Each add'l buffer has wpb spaces...
        auto x = p->next;
        while (x) {
            avail += _wordsPerBuffer;
            x = x->next;
        }
        return avail;
    }

private:
    // Can't use std::list because we need to put in RAM for IRQ use, so roll our own
    void __not_in_flash_func(_addToList)(AudioBuffer **list, AudioBuffer *element) {
        noInterrupts();
        // Find end of list, if any
        while ((*list) && ((*list)->next != nullptr)) {
            list = &(*list)->next;
        }
        if (*list) {
            (*list)->next = element;
        } else {
            *list = element;
        }
        element->next = nullptr;  // Belt and braces
        interrupts();
    }

    AudioBuffer *__not_in_flash_func(_takeFromList)(AudioBuffer **list) {
        noInterrupts();
        auto ret = *list;
        if (ret) {
            *list = ret->next;
        }
        interrupts();
        return ret;
    }

    void _deleteAudioBuffer(AudioBuffer *ab) {
        delete[] ab->buff;
        delete ab;
    }

    uint32_t *getResampledBuffer() {
        // Prime the pump when we're all out of frames
        if (!_filled && _cb) {
            _cb(_cbData);
        }

        if (!_filled) {
            for (size_t i = 0; i < _outputBufferWords; i++) {
                _outputBuffer[i] = _silenceSample;
                _underflow = true;
            }
            return _outputBuffer;  // Nothing to send in but the sound of silence...
        }

        uint32_t *p = _outputBuffer;
        if (_outputRate == _inputRate) {
            for (size_t i = 0; i < _outputBufferWords; i++) {
                if (_readOff == _wordsPerBuffer) {
                    _addToList(&_empty, _takeFromList(&_filled));
                    _readOff = 0;
                    if (_cb) {
                        _cb(_cbData);
                    }
                }
                if (!_filled) {
                    _underflow = true;
                    digitalWrite(LED_BUILTIN, LOW);
                }
                *(p++) = _filled ? _filled->buff[_readOff++] : _silenceSample;
            }
        } else {
            // _readOff will be a 16p16 fixed point
            for (size_t i = 0; i < _outputBufferWords; i++) {
                while (_readOff >= _wordsPerBuffer << 16) {
                    _addToList(&_empty, _takeFromList(&_filled));
                    _readOff -= _wordsPerBuffer << 16;
                    if (_cb) {
                        _cb(_cbData);
                    }
                }
                if (!_filled) {
                    _underflow = true;
                    digitalWrite(LED_BUILTIN, LOW);
                }
                *(p++) = _filled ? _filled->buff[_readOff >> 16] : _silenceSample;
                if (_filled) {
                    _readOff += _resample16p16;
                }
            }
        }
        return _outputBuffer;
    }

private:
    typedef struct AudioBuffer {
        struct AudioBuffer *next;
        uint32_t *buff;
    } AudioBuffer;
    AudioBuffer *_filled = nullptr;  // List of buffers ready to be played
    AudioBuffer *_empty = nullptr;   // List of buffers waiting to be filled. *_empty = currently writing

    size_t _bufferCount;
    size_t _wordsPerBuffer;
    int32_t _silenceSample;
    uint32_t _inputRate;
    uint32_t _outputRate;
    bool _running;
    size_t _userOff;
    size_t _readOff;
    uint32_t _resample16p16;  // Incremental step to get from inputRate to outputRate in fixed point 16.16 format
    bool _underflow;
    size_t _outputBufferWords;
    uint32_t *_outputBuffer;
    void (*_cb)(void *);
    void *_cbData;
};


template<size_t _outWords = 512>
class BackgroundAudioMixer {
public:
    BackgroundAudioMixer(AudioOutputBase &d, int outputRate) {
        _running = false;
        _out = &d;
        _outRate = outputRate;
        _running = false;
    }
    ~BackgroundAudioMixer() { /* Noop */
    }

    BackgroundAudioMixerInput *add() {
        auto x = new BackgroundAudioMixerInput(_outRate, _outWords);
        _input.push_back(x);
        return x;
    }

    bool begin() {
        if (_running) {
            return false;
        }

        // We will use natural frame size to minimize mismatch
        _out->setBuffers(4, _outWords);        // Framelen is in samples, but buffers takes words, which means 2 samples per word so we're good!
        _out->onTransmit(&_cb, (void *)this);  // The pump we will use to generate our audio
        _out->setBitsPerSample(16);
        _out->setStereo(true);
        _out->setFrequency(_outRate);
        _out->begin();

        // Stuff with silence to start
        uint32_t zeros[32] __attribute__((aligned(4))) = {};
        while (_out->availableForWrite() > 32) {
            _out->write((uint8_t *)zeros, sizeof(zeros));
        }

        _running = true;

        return true;
    }

private:
    static void _cb(void *ptr) {
        ((BackgroundAudioMixer *)ptr)->pump();
    }

    void generateOneFrame() {
        // Collect all the input leg buffers
        int16_t *leg[_input.size()];
        for (size_t i = 0; i < _input.size(); i++) {
            leg[i] = (int16_t *)_input[i]->getResampledBuffer();
        }

        // Sum them up with saturating arithmetic
        for (size_t i = 0; i < _outWords * 2; i++) {
            int32_t sum = 0;
            for (size_t j = 0; j < _input.size(); j++) {
                sum += leg[j][i];
            }
            if (sum > 32767) {
                sum = 32767;
            } else if (sum < -32767) {
                sum = -32767;
            }
            _outBuff[i] = (int16_t)sum;
        }
    }

    void pump() {
        while (_out->availableForWrite() >= (int)_outWords) {
            generateOneFrame();
            _out->write((uint8_t *)_outBuff, _outWords * 4);
        }
    }
    bool _running;
    AudioOutputBase *_out;
    int _outRate;
    int16_t _outBuff[_outWords * 2];
    std::vector<BackgroundAudioMixerInput *> _input;
};
