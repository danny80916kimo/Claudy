#pragma once
#include "lvgl.h"

// Thin facade over lv_canvas drawing for mascot.cpp.
// All coordinates are local to the canvas.
void gfx_fill_bg(lv_obj_t *canvas, lv_color_t c);
void gfx_fill_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t c);

// Convert an RGB565 uint16_t (our palette constants) to lv_color_t.
static inline lv_color_t rgb565_to_lv(uint16_t v) {
  return lv_color_make((v >> 11 & 0x1F) << 3,
                       (v >> 5  & 0x3F) << 2,
                       (v       & 0x1F) << 3);
}
