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

/**
    @brief Interrupt-safe, multicore-safe biftable buffer for libmad raw data.

    @tparam bytes Number of bytes to statically allocate for the raw buffer
*/
template <size_t bytes>
class RawDataBuffer {
public:
    RawDataBuffer() {
        _len = 0;
#if defined(ESP32)
        _mtx = portMUX_INITIALIZER_UNLOCKED;
#else
        mutex_init(&_mtx);
#endif
    }

    ~RawDataBuffer() {
        /* no op */
    }

    /**
        @brief Get access to internal buffer pointer (avoiding memcpy)

        @return Buffer pointer
    */
    inline uint8_t *buffer() {
        return _buff;
    }

    /**
        @brief Determine number of bytes that can be read

        @return Available bytes
    */
    inline size_t available() {
        return _len;
    }

    /**
        @brief Determine how much unused space is available in the buffer

        @return Free bytes
    */
    inline size_t availableForWrite() {
        return count - _len;
    }

    /**
        @brief Get total size of the statically allocated buffer

        @return Number of bytes in total
    */
    inline constexpr size_t size() {
        return count;
    }

    /**
        @brief Copy a block of memory into the buffer. Will not block

        @param [in] data Raw data bytes
        @param [in] cnt Number of bytes to write

        @return Number of bytes actually written
    */
    inline size_t write(const uint8_t *data, size_t cnt) {
#if defined(ESP32)
        taskENTER_CRITICAL(&_mtx);
#else
        noInterrupts();
        mutex_enter_blocking(&_mtx);
#endif
        size_t maxWritable = count - _len;
        size_t toWrite = std::min(cnt, maxWritable);
        memcpy(_buff + _len, data, toWrite);
        _len += toWrite;
#if defined(ESP32)
        taskEXIT_CRITICAL(&_mtx);
#else
        mutex_exit(&_mtx);
        interrupts();
#endif
        return toWrite;
    }
    /**
        @brief 0-fill a portion of the buffer

        @param [in] cnt Number of 0 bytes to write

        @return Number of bytes actually written
    */
    inline size_t write0(size_t cnt) {
#if defined(ESP32)
        taskENTER_CRITICAL(&_mtx);
#else
        noInterrupts();
        mutex_enter_blocking(&_mtx);
#endif
        size_t maxWritable = count - _len;
        size_t toWrite = std::min(cnt, maxWritable);
        bzero(_buff + _len, toWrite);
        _len += toWrite;
#if defined(ESP32)
        taskEXIT_CRITICAL(&_mtx);
#else
        mutex_exit(&_mtx);
        interrupts();
#endif
        return toWrite;
    }

    /**
        @brief Invalidate a portion of buffer and shift remaining data up

        @details
        Decoders need to be able to read an entire frame's worth of raw data from a contiguous memory region.  They
        all operate by using the internal buffer pointer plus an offset, which they track, to avoid having to shift
        old data out every single frame (i.e. avoiding `memcpy`s).  When they have accumulated enough of a virtual
        shift in their reading, they call `shiftUp` to actually do the memory copy and invalidate that old portion
        of the buffer.

        In general decoders want to call this function ad *infrequently* as possible.
    */
    inline void shiftUp(size_t cnt) {
#if defined(ESP32)
        taskENTER_CRITICAL(&_mtx);
#else
        noInterrupts();
        mutex_enter_blocking(&_mtx);
#endif
        if (cnt <= _len) {
            size_t toShift = _len - cnt;
            memmove(_buff, _buff + cnt, toShift);
            _len -= cnt;
        } else {
            _len = 0;
        }
#if defined(ESP32)
        taskEXIT_CRITICAL(&_mtx);
#else
        mutex_exit(&_mtx);
        interrupts();
#endif
    }

    /**
        @brief Throw out any data in the buffer
    */
    inline void flush() {
        _len  = 0;
    }

private:
    static const size_t count = bytes;
    uint8_t _buff[count];
    size_t _len;
#if defined(ESP32)
    portMUX_TYPE _mtx;
#else
    mutex_t _mtx;
#endif
};


/**
    @brief Special-purpose buffer which never shifts memory and only allows a single written block of data, used for ROM sources.
*/
class ROMDataBuffer {
public:
    ROMDataBuffer() {
        _buff = nullptr;
        _len = 0;
        _count = 0;
#if defined(ESP32)
        _mtx = portMUX_INITIALIZER_UNLOCKED;
#else
        mutex_init(&_mtx);
#endif
    }

    ~ROMDataBuffer() {
        /* no op */
    }

    /**
        @brief Get access to internal buffer pointer (avoiding memcpy)

        @return Buffer pointer
    */
    inline const uint8_t *buffer() {
        return _buff;
    }

    /**
        @brief Determine number of bytes that can be read

        @return Available bytes
    */
    inline size_t available() {
        return _len;
    }

    /**
        @brief Determine how much unused space is available in the buffer

        @return 0 because this buffer can never really be written (but `write` does work)
    */
    inline size_t availableForWrite() {
        return 0;
    }

    /**
        @brief Get total size of the statically allocated buffer

        @return Number of bytes in total
    */
    inline constexpr size_t size() {
        return _count;
    }

    /**
        @brief Copy a *pointer* to a block of memory into the buffer, replacing any existing data

        @details
        This differs from `RawDataBuffer` in that no data is actually copied and there is no local buffer.  Only
        the pointer to the ROM data and its size is captured here.  The same `ROMDataBuffer` can have `write`
        called multiple times as long as all the existing data is already emptied by the decoder or cleared
        using `flush`.

        @param [in] data Raw data bytes
        @param [in] cnt Number of bytes to write

        @return Number of bytes actually written
    */
    inline size_t write(const uint8_t *data, size_t cnt) {
        _buff = data;
        _len = cnt;
        _count = cnt;
        return cnt;
    }

    /**
        @brief 0-fill a portion of the buffer, but will fail because there is no buffer here

        @param [in] cnt Number of 0 bytes to write

        @return 0 due to unsupported
    */
    inline size_t write0(size_t cnt) {
        return 0;
    }


    /**
        @brief Invalidate a portion of buffer and shift remaining data up

        @details
        Decoders need to be able to read an entire frame's worth of raw data from a contiguous memory region.  They
        all operate by using the internal buffer pointer plus an offset, which they track, to avoid having to shift
        old data out every single frame (i.e. avoiding `memcpy`s).  When they have accumulated enough of a virtual
        shift in their reading, they call `shiftUp` to actually do the memory copy and invalidate that old portion
        of the buffer.

        Because this is a ROM image, we don't actually have to `memcpy` and just adjust the internal pointer.

        In general decoders want to call this function ad *infrequently* as possible.
    */
    inline void shiftUp(size_t cnt) {
#if defined(ESP32)
        taskENTER_CRITICAL(&_mtx);
#else
        noInterrupts();
        mutex_enter_blocking(&_mtx);
#endif
        if (cnt <= _len) {
            _buff += cnt;
            _len -= cnt;
        } else {
            _len = 0;
        }
#if defined(ESP32)
        taskEXIT_CRITICAL(&_mtx);
#else
        mutex_exit(&_mtx);
        interrupts();
#endif
    }

    /**
        @brief Throw away (flush) the input buffer
    */
    inline void flush() {
        _len  = 0;
    }

private:
    const uint8_t *_buff;
    size_t _len;
    size_t _count;
#if defined(ESP32)
    portMUX_TYPE _mtx;
#else
    mutex_t _mtx;
#endif
};
