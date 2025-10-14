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

#include <driver/i2s_pdm.h>
#include "ESP32I2SAudio.h"
#include "WrappedAudioOutputBase.h"

/**
     @brief PDM object with IRQ-based callbacks to a FreeRTOS task, for use with BackgroundAudio
*/
class ESP32PDMAudio : public ESP32I2SAudio {
public:
    /**
         @brief Construct ESP32-based PDM object with IRQ-based callbacks to a FreeRTOS task, for use with BackgroundAudio

         @param [in] dout GPIO pin to use as the DOUT from the PDM device
    */
    ESP32PDMAudio(int8_t dout = 0) {
        _dout = dout;
        _running = false;
        _sampleRate = 44100;
        _buffers = 5;
        _bufferWords = 512;
        _silenceSample = 0;
        _cb = nullptr;
        _underflowed = false;
    }

    virtual ~ESP32PDMAudio() {
    }

    /**
        @brief Set the PDM GPIO pin before calling `begin`

        @param [in] dout GPIO pin to use as the PDM output (connected to the LPF and speaker/amp)
    */
    void setPin(int8_t dout) {
        _dout = dout;
    }

    /**
        @brief Set the sample rate of the PDM interface.  Can be called while running

        @param [in] freq New sampling frequency in hertz

        @return True if succeeded
    */
    bool setFrequency(int freq) override {

        // TODO - There is some fixed off-by-ratio 1/1.25 in the PDM output clock vs. the PCM input data at IDF 5.5
        freq *= 8;
        freq /= 10;
        if (_running && (_sampleRate != freq)) {
            i2s_pdm_tx_clk_config_t clk_cfg;
            clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG((uint32_t)freq);
            clk_cfg.up_sample_fp = 960;
            clk_cfg.up_sample_fs = 480;
            i2s_channel_disable(_tx_handle);
            i2s_channel_reconfig_pdm_tx_clock(_tx_handle, &clk_cfg);
            i2s_channel_enable(_tx_handle);
        }
        _sampleRate = freq;
        return true;
    }

    /**
        @brief Start the PDM interface

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

        i2s_pdm_tx_config_t pdm_cfg = {
            .clk_cfg = I2S_PDM_TX_CLK_DEFAULT_CONFIG(_sampleRate),
            .slot_cfg = I2S_PDM_TX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .clk = I2S_GPIO_UNUSED,
                .dout = (gpio_num_t)_dout,
                .dout2 = I2S_GPIO_UNUSED,
                .invert_flags = {
                    .clk_inv = false,
                },
            },
        };
        pdm_cfg.slot_cfg.data_fmt = I2S_PDM_DATA_FMT_PCM;
        pdm_cfg.clk_cfg.up_sample_fp = 960;
        pdm_cfg.clk_cfg.up_sample_fs = 480;
        assert(ESP_OK == i2s_channel_init_pdm_tx_mode(_tx_handle, &pdm_cfg));

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

};
