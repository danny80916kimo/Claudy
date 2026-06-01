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

// Lower half — info region (Option A: compact state+% row, big 2-line message, bar)
constexpr int INFO_X   = 20;
constexpr int INFO_W   = 440;       // 20px padding each side
constexpr int STATE_Y  = 250;       // state name (left) + token % (right), 28px
constexpr int TOOL_Y   = 292;       // tool chip + tool name
constexpr int CHIP_W   = 36;
constexpr int CHIP_H   = 28;
constexpr int MSG_Y    = 330;       // message: 24px CJK, up to 2 lines
constexpr int PCT_W    = 130;       // token % label (right-aligned beside the bar)
constexpr int BAR_Y    = 446;
constexpr int BAR_H    = 16;
constexpr int BAR_W    = 298;       // bar width; big % sits to its right, detail above

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
