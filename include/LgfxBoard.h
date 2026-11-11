#pragma once

#if defined(BOARD_T_CONNECT_PRO)

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// LILYGO T-Connect-Pro: ST7796 480x222 + CST226SE Touch
class LgfxBoard : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;
  lgfx::Touch_CST226 _touch;

public:
  LgfxBoard() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 12;
      cfg.pin_mosi = 11;
      cfg.pin_miso = 13;
      cfg.pin_dc = 41;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      // ST7796 GRAM 320×480, sichtbares Panel 222×480, zentriert (offset 49)
      auto cfg = _panel.config();
      cfg.pin_cs = 21;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;
      cfg.memory_width = 320;
      cfg.memory_height = 480;
      cfg.panel_width = 222;
      cfg.panel_height = 480;
      cfg.offset_x = 49;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel.config(cfg);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = 46;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    {
      auto cfg = _touch.config();
      cfg.x_min = 0;
      cfg.x_max = 479;
      cfg.y_min = 0;
      cfg.y_max = 221;
      cfg.pin_int = 3;
      cfg.pin_rst = 47;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 0;
      cfg.i2c_addr = 0x5A;
      cfg.pin_sda = 39;
      cfg.pin_scl = 40;
      cfg.freq = 400000;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};

#endif
