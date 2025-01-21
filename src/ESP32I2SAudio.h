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
#include "WrappedAudioOutputBase.h"

class ESP32I2SAudio : public AudioOutputBase {
public:
    ESP32I2SAudio(int8_t bclk = 0, int8_t ws = 1, int8_t dout = 2, int8_t mclk = -1) {
        _bclk = bclk;
        _ws = ws;
        _dout = dout;
        _mclk = mclk;
        _running = false;
        _sampleRate = 44100;
        _buffers = 5;
        _bufferWords = 512;
        _silenceSample = 0;
        _cb = nullptr;
    }

    virtual ~ESP32I2SAudio() {
    }

    void setPins(int8_t bclk, int8_t ws, int8_t dout, int8_t mclk = -1) {
        _bclk = bclk;
        _ws = ws;
        _dout = dout;
        _mclk = mclk;
    }

    void setInverted(bool bclk, bool ws, bool mclk = false) {
        _bclkInv = bclk;
        _wsInv = ws;
        _mclkInv = mclk;
    }

    bool setBuffers(size_t buffers, size_t bufferWords, int32_t silenceSample = 0) override {
        if (!_running) {
            _buffers = buffers;
            _bufferWords = bufferWords;
            _silenceSample = silenceSample;
        }
        return !_running;
    }

    bool setBitsPerSample(int bps) override {
        if (!_running && bps == 16)  {
            return true;
        }
        return false;
    }

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

    bool setStereo(bool stereo = true) override {
        return stereo;
    }

    bool begin() override {
        if (_running) {
            return false;
        }

        // Make a new channel of the requested buffers (which may be ignored by the IDF!)
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
        chan_cfg.dma_desc_num = _buffers;
        chan_cfg.dma_frame_num = _bufferWords * 4;
        i2s_new_channel(&chan_cfg, &_tx_handle, nullptr);

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
        i2s_channel_init_std_mode(_tx_handle, &std_cfg);

        // Prefill silence and calculate how bug we really have
        int16_t a[2] = {0,0};
        size_t written = 0;
        do {
          i2s_channel_preload_data(_tx_handle, (void*)a, sizeof(a), &written);
          _totalAvailable += written;
        } while (written);

        // The IRQ callbacks which will just trigger the playback task
        i2s_event_callbacks_t _cbs = {
            .on_recv = nullptr,
            .on_recv_q_ovf = nullptr,
            .on_sent = _onSent,
            .on_send_q_ovf = _onSentUnder
        };
        i2s_channel_register_event_callback(_tx_handle, &_cbs, (void *)this);
        xTaskCreate(_taskShim, "BackgroundAudioI2S", 4096, (void*)this, 2, &_taskHandle);
        _running = ESP_OK == i2s_channel_enable(_tx_handle);
        return _running;
    }

    static bool _onSent(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
      return ((ESP32I2SAudio *)user_ctx)->_onSentCB(handle, event);
    }


    static bool _onSentUnder(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
      return ((ESP32I2SAudio *)user_ctx)->_onSentCB(handle, event, true);
    }

    static void _taskShim(void *pvParameters) {
        ((ESP32I2SAudio *)pvParameters)->_backgroundTask();
    }

    void _backgroundTask() {
        while (true) {
            uint32_t ulNotifiedValue;
            xTaskNotifyWait(0, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
            _frames++;
            int32_t size = (int32_t)ulNotifiedValue;
            if (size < 0) {
                _underflows++;
                size = -size;
            }
            _available += size;
            if (_available > _totalAvailable) {
              _available = _totalAvailable;
            }
            if (_cb) {
                _cb(_cbData);
            }
        }
    }

    uint32_t _irqs = 0;
    uint32_t _frames = 0;
    uint32_t _underflows = 0;

    bool _onSentCB(i2s_chan_handle_t handle, i2s_event_data_t *event, bool underflow = false) {
        BaseType_t xHigherPriorityTaskWoken;
        xHigherPriorityTaskWoken = pdFALSE;
        if (_taskHandle) {
            _irqs++;
            xTaskNotifyFromISR(_taskHandle, event->size * (underflow ? -1 : 1), eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
        }
        return (bool)xHigherPriorityTaskWoken;
    }


    bool end() override {
        // TODO
        return false;
    }

    bool getUnderflow() override {
        return false; // TODO
    }

    void onTransmit(void(*cb)(void *), void *cbData) override {
        noInterrupts();
        _cb = cb;
        _cbData = cbData;
        interrupts();
    }

    // From Print
    size_t write(const uint8_t *buffer, size_t size) override {
      size_t written = 0;
      i2s_channel_write(_tx_handle, buffer, size, &written, 0);
      noInterrupts(); // TODO - Freertos task protection instead?
      if (written != size) {
        _available = 0;
      } else if (_available >= written) {
        _available -= written;
      } else {
        _available = 0;
      }
      interrupts();
      return  written;
    }

    size_t write(uint8_t d) override {
        return 0; // No bytes!
    }

    int availableForWrite() override {
        return _available; // It's our best guess for now
    }

private:
    bool _running;
    int8_t _bclk = 0;
    int8_t _ws = 1;
    int8_t _dout = 2;
    int8_t _mclk = -1;
    bool _bclkInv = false;
    bool _wsInv = false;
    bool _mclkInv = false;
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
    size_t _available = 0;
};
