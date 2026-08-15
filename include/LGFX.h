#pragma once

#include <LovyanGFX.hpp>

#include "DisplayConfig.h"

class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7796 _panel;
    lgfx::Bus_SPI _bus;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus.config();
            cfg.spi_host = SPI2_HOST;
            cfg.freq_write = 20000000;
            cfg.freq_read = 8000000;
            cfg.spi_mode = 0;
            cfg.pin_miso = -1;
            cfg.pin_mosi = TFT_PIN_MOSI;
            cfg.pin_sclk = TFT_PIN_SCLK;
            cfg.pin_dc = TFT_PIN_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }

        {
            auto cfg = _panel.config();
            cfg.pin_cs = TFT_PIN_CS;
            cfg.pin_rst = TFT_PIN_RST;
            cfg.pin_busy = -1;
            cfg.panel_width = TFT_WIDTH;
            cfg.panel_height = TFT_HEIGHT;
            cfg.memory_width = TFT_WIDTH;
            cfg.memory_height = TFT_HEIGHT;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            _panel.config(cfg);
        }

        setPanel(&_panel);
    }
};
