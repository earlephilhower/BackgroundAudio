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

// Interrupt-safe, multicore-safe biftable buffer for libmad raw data.
class RawDataBuffer {
public:
    RawDataBuffer() {
        _len = 0;
        mutex_init(&_mtx);
    }

    ~RawDataBuffer() {
        /* no op */
    }

    inline uint8_t *buffer() {
        return _buff;
    }

    inline size_t available() {
        return _len;
    }

    inline size_t availableForWrite() {
        return count - _len;
    }

    inline constexpr size_t size() {
        return count;
    }

    inline size_t write(const uint8_t *data, size_t cnt) {
        noInterrupts();
        mutex_enter_blocking(&_mtx);
        size_t maxWritable = count - _len;
        size_t toWrite = std::min(cnt, maxWritable);
        memcpy(_buff + _len, data, toWrite);
        _len += toWrite;
        mutex_exit(&_mtx);
        interrupts();
        return toWrite;
    }

    inline size_t write0(size_t cnt) {
        noInterrupts();
        mutex_enter_blocking(&_mtx);
        size_t maxWritable = count - _len;
        size_t toWrite = std::min(cnt, maxWritable);
        bzero(_buff + _len, toWrite);
        _len += toWrite;
        mutex_exit(&_mtx);
        interrupts();
        return toWrite;
    }

    inline void shiftUp(size_t cnt) {
        noInterrupts();
        mutex_enter_blocking(&_mtx);
        if (cnt <= _len) {
            size_t toShift = _len - cnt;
            memmove(_buff, _buff + cnt, toShift);
            _len -= cnt;
        } else {
            _len = 0;
        }
        mutex_exit(&_mtx);
        interrupts();
    }

    inline void flush() {
        _len  = 0;
    }

private:
    static const size_t count = 8 * 1024;
    uint8_t _buff[count];
    size_t _len;
    mutex_t _mtx;
};


class ROMDataBuffer {
public:
    ROMDataBuffer() {
        _buff = nullptr;
        _len = 0;
        _count = 0;
        mutex_init(&_mtx);
    }

    ~ROMDataBuffer() {
        /* no op */
    }

    inline const uint8_t *buffer() {
        return _buff;
    }

    inline size_t available() {
        return _len;
    }

    inline size_t availableForWrite() {
        return 0;
    }

    inline constexpr size_t size() {
        return _count;
    }

    inline size_t write(const uint8_t *data, size_t cnt) {
        _buff = data;
        _len = cnt;
        _count = cnt;
        return cnt;
    }

    inline size_t write0(size_t cnt) {
        return 0;
    }

    inline void shiftUp(size_t cnt) {
        noInterrupts();
        mutex_enter_blocking(&_mtx);
        if (cnt <= _len) {
            _buff += cnt;
            _len -= cnt;
        } else {
            _len = 0;
        }
        mutex_exit(&_mtx);
        interrupts();
    }

    inline void flush() {
        _len  = 0;
    }

private:
    const uint8_t *_buff;
    size_t _len;
    size_t _count;
    mutex_t _mtx;
};
