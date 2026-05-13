#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// T-Display-S3: 170x320 ST7789, 8-bit parallel (i80) bus.
// Pins from LilyGo schematic (revision with USB-C, 16MB flash, PSRAM).
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789  _panel;
  lgfx::Bus_Parallel8 _bus;
  lgfx::Light_PWM     _light;
public:
  LGFX();
};

extern LGFX tft;
extern lgfx::LGFX_Sprite canvas;

void displayBegin(uint8_t brightness);
void renderFrame();         // full re-render, allocate sprite if needed
void requestRedraw();       // mark dirty, will redraw on next renderFrame()
