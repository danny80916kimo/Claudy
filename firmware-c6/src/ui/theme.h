#pragma once
#include <stdint.h>
#include "../hw/pins.h"

// Layout (480x480 screen, layout B from spec: upper half mascot, lower half info).
// Derive from the pins.h panel resolution so there's a single source of truth.
constexpr int LCD_W = LCD_H_RES;
constexpr int LCD_H = LCD_V_RES;

// Upper half — mascot canvas region
constexpr int MASCOT_X = (480 - 400) / 2;   // = 40 (centered; art centers within)
constexpr int MASCOT_Y = 0;
constexpr int MASCOT_W = 400;
constexpr int MASCOT_H = 240;     // 400x240 = 192KB canvas

// Lower half — info region
constexpr int INFO_X = 24;
constexpr int INFO_Y = 256;
constexpr int INFO_W = 432;       // padding 24 each side
constexpr int INFO_H = 200;       // 256..456
constexpr int BAR_Y  = 420;
constexpr int BAR_H  = 18;

// RGB565 palette (same constants as firmware/mascot.cpp + display.cpp)
constexpr uint16_t COL_BG       = 0x0000;   // black
constexpr uint16_t COL_FG       = 0xFFFF;   // white
constexpr uint16_t COL_DIM      = 0xBDF7;   // light gray
constexpr uint16_t COL_BAR_OK   = 0x06DF;   // teal
constexpr uint16_t COL_BAR_WARN = 0xFD20;   // orange
constexpr uint16_t COL_BAR_HOT  = 0xF800;   // red

// Mascot palette
constexpr uint16_t COL_CLAUDE_ORANGE = 0xCC2D;
constexpr uint16_t COL_CLAUDE_DARK   = 0x1082;
constexpr uint16_t COL_CLAUDE_DIM    = 0x4208;
constexpr uint16_t COL_ACCENT_THINK  = 0x7E5F;
constexpr uint16_t COL_ACCENT_WORK   = 0x05FF;
constexpr uint16_t COL_ACCENT_WAIT   = 0xFEC0;
constexpr uint16_t COL_ACCENT_ERROR  = 0xF820;
constexpr uint16_t COL_ACCENT_DONE   = 0x07E5;
