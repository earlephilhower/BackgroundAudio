/*
    BackgroundAudio
    Plays an audio file using IRQ driven decompression.  Main loop() writes
    data to the buffer but isn't blocked while playing

    Copyright (c) 2025 Earle F. Philhower, III <earlephilhower@yahoo.com>

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

#include <driver/i2s_std.h>
#include <atomic>
#include "WrappedAudioOutputBase.h"

/**
    ESP-IDF has an error in initializer order for the I2S clock default macros.
    Redefine it in the proper order until this is fixed.
    See https://github.com/espressif/arduino-esp32/issues/10975
    and https://github.com/espressif/esp-idf/issues/15405
*/

#if SOC_I2S_HW_VERSION_2
#undef I2S_STD_CLK_DEFAULT_CONFIG
#define I2S_STD_CLK_DEFAULT_CONFIG(rate) \
  { .sample_rate_hz = rate, .clk_src = I2S_CLK_SRC_DEFAULT, .ext_clk_freq_hz = 0, .mclk_multiple = I2S_MCLK_MULTIPLE_256, }
#endif

/**
     @brief I2S object with IRQ-based callbacks to a FreeRTOS task, for use with BackgroundAudio
*/
class ESP32I2SAudio : public AudioOutputBase {
public:
    /**
         @brief Construct ESP32-based I2S object with IRQ-based callbacks to a FreeRTOS task, for use with BackgroundAudio

         @param [in] bclk GPIO pin to use for the bit clock (BCLK)
         @param [in] ws GPIO pin to use for the word clock (LRCLK, WS)
         @param [in] dout GPIO pin to use as the DOUT from the I2S device (connected to DIN on the DAC)
         @param [in] mclk Optional GPIO pin for the MCLK (multiplied clock) output, not always needed
    */
    ESP32I2SAudio(int8_t bclk = 0, int8_t ws = 1, int8_t dout = 2, int8_t mclk = -1) {
        _bclk = bclk;
        _ws = ws;
        _dout = dout;
        _mclk = mclk;
        _running = false;
        _sampleRate = 44100;
        _buffers = 5;
        _bufferWords = 1023;
        _silenceSample = 0;
        _cb = nullptr;
        _underflowed = false;
    }

    virtual ~ESP32I2SAudio() {
    }

    /**
        @brief Set the I2S GPIO pins before calling `begin`

        @param [in] bclk GPIO pin to use for the bit clock (BCLK)
        @param [in] ws GPIO pin to use for the word clock (LRCLK, WS)
        @param [in] dout GPIO pin to use as the DOUT from the I2S device (connected to DIN on the DAC)
        @param [in] mclk Optional GPIO pin for the MCLK (multiplied clock) output, not always needed
    */
    void setPins(int8_t bclk, int8_t ws, int8_t dout, int8_t mclk = -1) {
        _bclk = bclk;
        _ws = ws;
        _dout = dout;
        _mclk = mclk;
    }

    /**
        @brief Set the I2S GPIO inversions before calling `begin`

        @param [in] bclk True to invert BCLK output
        @param [in] ws True to invert WS/LRCLK output
        @param [in] mclk Optionallt, true to invert the MCLK output
    */
    void setInverted(bool bclk, bool ws, bool mclk = false) {
        _bclkInv = bclk;
        _wsInv = ws;
        _mclkInv = mclk;
    }

    /**
        @brief Set the size and number of the I2S buffers before `begin`

        @param [in] buffers Number of I2S DMA buffers
        @param [in] bufferWords Number of 32-bit words (i.e. a single stereo 16-bit sample) per each DMA buffer
        @param [in] silenceSample Optional 32-bit value to send out in case of underflow, normally 0

        @return True if parameters were successful
    */
    bool setBuffers(size_t buffers, size_t bufferWords, int32_t silenceSample = 0) override {
        if (!_running) {
            _buffers = buffers;
            _bufferWords = bufferWords;
            // We need DMA buffers of 1023 4-byte samples or less. Adjust behind the scenes.
            // The upper level code will get 2x or 4x the IRQs, but availableForWrite() should
            // still report the proper space and let it do nothing for the extra IRQs.
            while (_bufferWords > 1023) {
                _buffers *= 2;
                _bufferWords /= 2;
            }
            _silenceSample = silenceSample;
        }
        return !_running;
    }

    /**
        @brief Set the bits per sample for the I2S output.  Only 16-bit supported

        @param [in] bps Bits per sample, only 16-bit supported

        @return True if successful
    */
    bool setBitsPerSample(int bps) override {
        if (!_running && bps == 16)  {
            return true;
        }
        return false;
    }

    /**
        @brief Set the sample rate (LRCLK/WS) of the I2S interface.  Can be called while running

        @param [in] freq New sampling frequency in hertz

        @return True if succeeded
    */
    bool setFrequency(int freq) override {
        if (_running && (_sampleRate != freq)) {
            i2s_std_clk_config_t clk_cfg;
            clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)freq);
            i2s_channel_disable(_tx_handle);
            i2s_channel_reconfig_std_clock(_tx_handle, &clk_cfg);
            i2s_channel_enable(_tx_handle);
        }
        _sampleRate = freq;
        return true;
    }

    /**
        @brief Set mono or stereo mode.  Only stereo supported

        @param [in] stereo Set to true for stereo (L/R) output

        @return True if success
    */
    bool setStereo(bool stereo = true) override {
        return stereo;
    }

    /**
        @brief Start the I2S interface

        @details
        Allocates an I2S hardware device with the requested number and size of DMA buffers and pinout.
        A FreeRTOS task is started, awoken from the I2S DMA buffer complete interrupt, to process the
        reading and writing and keep track of the available sample space for upper layers.

        @return True on success
    */
    bool begin() override {
        if (_running) {
            return false;
        }

        // Make a new channel of the requested buffers (which may be ignored by the IDF!)
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        chan_cfg.dma_desc_num = _buffers;
        chan_cfg.dma_frame_num = _bufferWords;
        assert(ESP_OK == i2s_new_channel(&chan_cfg, &_tx_handle, nullptr));

        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(_sampleRate),
            .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = _mclk < 0 ? I2S_GPIO_UNUSED : (gpio_num_t)_mclk,
                .bclk = (gpio_num_t)_bclk,
                .ws = (gpio_num_t)_ws,
                .dout = (gpio_num_t)_dout,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = {
                    .mclk_inv = _mclkInv,
                    .bclk_inv = _bclkInv,
                    .ws_inv = _wsInv,
                },
            },
        };
        assert(ESP_OK == i2s_channel_init_std_mode(_tx_handle, &std_cfg));

        i2s_chan_info_t _info;
        i2s_channel_get_info(_tx_handle, &_info);
        // If the IDF has changed our buffer size or count then we can't work
        assert(_info.total_dma_buf_size == _buffers * _bufferWords * 4);
        _totalAvailable = _info.total_dma_buf_size;

        // Prefill silence and calculate how bug we really have
        int16_t a[2] = {0, 0};
        size_t written = 0;
        do {
            i2s_channel_preload_data(_tx_handle, (void*)a, sizeof(a), &written);
        } while (written);

        // The IRQ callbacks which will just trigger the playback task
        i2s_event_callbacks_t _cbs = {
            .on_recv = nullptr,
            .on_recv_q_ovf = nullptr,
            .on_sent = _onSent,
            .on_send_q_ovf = nullptr
        };
        assert(ESP_OK == i2s_channel_register_event_callback(_tx_handle, &_cbs, (void *)this));
        xTaskCreate(_taskShim, "BackgroundAudioI2S", 8192, (void*)this, 2, &_taskHandle);
        _running = ESP_OK == i2s_channel_enable(_tx_handle);
        return _running;
    }

    /**
        @brief C-language wrapper for I2S Sent event

        @return True if a task was woken up (FreeRTOS xHigherPriorityTaskWoken)
    */
    static IRAM_ATTR bool _onSent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
        return ((ESP32I2SAudio *)user_ctx)->_onSentCB(handle, event);
    }

    /**
        @brief C-language shim to start the real object's task
    */
    static void _taskShim(void *pvParameters) {
        ((ESP32I2SAudio *)pvParameters)->_backgroundTask();
    }

    /**
        @brief Background I2S DMA buffer notification task.  Tracks number of bytes available to be written.
    */
    void _backgroundTask() {
        while (true) {
            // Pause the task until notification comes in from the IRQ callbacks
            uint32_t ulNotifiedValue;
            xTaskNotifyWait(0, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
            // One DMA transfer == one frame
            _frames++;
            // Use the notification value to transmit how much data was in the DMA block.  When negative this was an underflow notification
            // Saturating add: _available = min(_available + size, _totalAvailable)
            _saturating_add_available((uint32_t)ulNotifiedValue);
            // Callback used from within a normal FreeRTOS task, so it can be slow and/or write I2S
            if (_cb) {
                _cb(_cbData);
            }
        }
    }

    inline void _saturating_add_available(uint32_t add) {
        uint32_t cur = _available.load(std::memory_order_relaxed);
        do {
            uint64_t sum = (uint64_t)cur + add;
            uint32_t capped = sum;
            if (sum > _totalAvailable) {
                capped = (uint32_t) _totalAvailable;  // Cap to the total available
                _underflowed.store(true, std::memory_order_release);
                _underflows.fetch_add(1, std::memory_order_relaxed);
            }

            if (_available.compare_exchange_weak(cur, capped, std::memory_order_release, std::memory_order_relaxed)) {
                return;
            }
            // cur reloaded on failure
        } while (true);
    }

    inline void _saturating_sub_available(uint32_t sub) {
        uint32_t cur = _available.load(std::memory_order_relaxed);
        do {
            uint32_t next = (cur > sub) ? (cur - sub) : 0u;
            if (_available.compare_exchange_weak(cur, next, std::memory_order_release, std::memory_order_relaxed)) {
                return;
            }
        } while (true);
    }


    /**
             @brief Get number of DMA frames(buffers) processed

             @return Number of frames
    */
    uint32_t frames() {
        return _frames;
    }

    /**
                @brief Get the number of input data shifts processed by decoder since `begin`

                @return Number of times data has been shifted in the raw input buffer
    */
    uint32_t irqs() {
        return _irqs;
    }

    /**
              @brief Get the number of times the MP3 decoder has underflowed waiting on raw data since `begin`

              @return Number of frames of underflow data have occurred
    */
    uint32_t underflows() {
        return _underflows.load(std::memory_order_acquire);
    }

    /**
        @brief Object-based callback for I2S Sent notification

        @return True if a task was woken up (FreeRTOS xHigherPriorityTaskWoken)
    */
    IRAM_ATTR bool _onSentCB(i2s_chan_handle_t handle, i2s_event_data_t *event) {
        BaseType_t xHigherPriorityTaskWoken;
        xHigherPriorityTaskWoken = pdFALSE;
        if (_taskHandle) {
            _irqs++;
            xTaskNotifyFromISR(_taskHandle, event->size, eSetValueWithoutOverwrite, &xHigherPriorityTaskWoken);
        }
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
        return (bool)xHigherPriorityTaskWoken;
    }


    /**
        @brief Stop the I2S device

        @return True if success
    */
    bool end() override {
        if (_running) {
            i2s_channel_disable(_tx_handle);
            i2s_del_channel(_tx_handle);
            vTaskDelete(_taskHandle);
            _running = false;
        }
        return true;
    }

    /**
        @brief Determine if there was an underflow since the last time this was called.  Cleared on read.

        @return True if an underflow occurred.
    */
    bool getUnderflow() override {
        // Atomically read-and-clear
        return _underflowed.exchange(false, std::memory_order_acq_rel);
    }

    /**
        @brief Set the callback function to be called every DMA buffer completion

        @param [in] cb Callback function
        @param [in] cbData Data to be passed to the callback function
    */
    void onTransmit(void(*cb)(void *), void *cbData) override {
        noInterrupts();
        _cb = cb;
        _cbData = cbData;
        interrupts();
    }

    /**
        @brief Write data to the I2S interface.  Not legal from IRQ context.  Will not block and may write less than requested.

        @param [in] buffer Data to be written
        @param [in] size Number of bytes to write.

        @return Number of bytes actually written to the I2S DMA buffers
    */
    size_t write(const uint8_t *buffer, size_t size) override {
        // The ESP32 i2s_channel_write will stop at a DMA buffer end even if add'l DMA buffers are free
        // So explicitly loop until either we run out of DMA buffers or data to fill them with
        size_t cumWritten = 0;
        while (size) {
            size_t written = 0;
            i2s_channel_write(_tx_handle, buffer, size, &written, 100);
            _saturating_sub_available((uint32_t)written);
            buffer += written;
            size -= written;
            cumWritten += written;
            // If we're full-up, don't block just exit
            if (!written) {
                break;
            }
        }
        return cumWritten;
    }

    /**
        @brief Write single byte to I2S buffers.  Not supported

        @return 0 always
    */
    size_t write(uint8_t d) override {
        return 0; // No bytes!
    }

    /**
        @brief Determine the number of frames we can write to the DMA buffers at this instant

        @return Number of frames available.  May not be completely accurate
    */
    int availableForWrite() override {
        return (int)_available.load(std::memory_order_acquire);
    }

protected:
    bool _running;
    int8_t _bclk = 0;
    int8_t _ws = 1;
    int8_t _dout = 2;
    int8_t _mclk = -1;
    bool _bclkInv = false;
    bool _wsInv = false;
    bool _mclkInv = false;
    std::atomic<bool> _underflowed{false};
    size_t _sampleRate;
    size_t _buffers;
    size_t _bufferWords;
    uint32_t _silenceSample;
    TaskHandle_t _taskHandle = 0;
    void (*_cb)(void *);
    void *_cbData;
    // I2S IDF object and info
    i2s_chan_handle_t _tx_handle;
    size_t _totalAvailable = 0;
    std::atomic<uint32_t> _available{0};
    uint32_t _irqs = 0; // Number of I2S IRQs received
    uint32_t _frames = 0; // Number of DMA buffers sent
    std::atomic<uint32_t> _underflows{0}; // Number of underflowed DMA buffers
};
