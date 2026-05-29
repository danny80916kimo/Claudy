#include "mascot.h"
#include "gfx.h"
#include "theme.h"
#include "esp_heap_caps.h"
#include <Arduino.h>
#include <math.h>

namespace {
// 11 cols × 9 rows pixel grid. Same art as firmware/mascot.cpp.
constexpr int ROWS = 9;
constexpr int COLS = 11;

const char* CLAUDY_OPEN[] = {
  "           ",
  " ######### ",
  " ######### ",
  " ##.###.## ",
  "###.###.###",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
const char* CLAUDY_BLINK[] = {
  "           ",
  " ######### ",
  " ######### ",
  " ######### ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
const char* CLAUDY_SQUINT[] = {
  "           ",
  " ######### ",
  " ##.###.## ",
  " ##.###.## ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};
const char* CLAUDY_X_EYES[] = {
  "           ",
  " ######### ",
  " ##.###.## ",
  " ##.###.## ",
  "###########",
  "###########",
  " ######### ",
  " # #   # # ",
  " # #   # # ",
};

void draw_claudy(lv_obj_t *canvas, int cx, int cy, int px,
                 const char* const* grid,
                 lv_color_t body, lv_color_t eye, int offX, int offY) {
  const int x0 = cx - (COLS * px) / 2 + offX;
  const int y0 = cy - (ROWS * px) / 2 + offY;
  for (int r = 0; r < ROWS; r++) {
    const char* row = grid[r];
    for (int c = 0; c < COLS; c++) {
      char ch = row[c];
      if (ch == '#')      gfx_fill_rect(canvas, x0 + c * px, y0 + r * px, px, px, body);
      else if (ch == '.') gfx_fill_rect(canvas, x0 + c * px, y0 + r * px, px, px, eye);
    }
  }
}
}  // namespace

lv_obj_t* mascot_create(lv_obj_t *parent) {
  const size_t bytes = MASCOT_W * MASCOT_H * 2;
  void *buf = heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!buf) {
    Serial.printf("mascot_create: alloc fail (%u bytes)\n", (unsigned)bytes);
    return nullptr;
  }
  lv_obj_t *canvas = lv_canvas_create(parent);
  lv_canvas_set_buffer(canvas, buf, MASCOT_W, MASCOT_H, LV_IMG_CF_TRUE_COLOR);
  lv_obj_set_pos(canvas, MASCOT_X, MASCOT_Y);
  gfx_fill_bg(canvas, lv_color_make(0, 0, 0));
  Serial.printf("mascot_create: canvas %dx%d (%u bytes)\n",
                MASCOT_W, MASCOT_H, (unsigned)bytes);
  return canvas;
}

uint32_t mascot_anim_interval(MascotState s) {
  switch (s) {
    case STATE_BOOT:     return 0;
    case STATE_IDLE:     return 120;
    case STATE_THINKING: return 100;
    case STATE_WORKING:  return 80;
    case STATE_WAITING:  return 120;
    case STATE_ERROR:    return 60;
    case STATE_DONE:     return 100;
  }
  return 0;
}

void mascot_draw(lv_obj_t *canvas, MascotState state) {
  if (!canvas) return;
  gfx_fill_bg(canvas, lv_color_make(0, 0, 0));

  const int cx = MASCOT_W / 2;
  const int cy = MASCOT_H / 2;

  // Largest pixel size that fits with margin.
  int px = 16;
  if (px * COLS > MASCOT_W - 20) px = (MASCOT_W - 20) / COLS;
  if (px * ROWS > MASCOT_H - 20) px = (MASCOT_H - 20) / ROWS;

  unsigned long t = millis();
  int dx = 0, dy = 0;
  lv_color_t body = rgb565_to_lv(COL_CLAUDE_ORANGE);
  lv_color_t eye  = rgb565_to_lv(COL_CLAUDE_DARK);
  const char* const* grid = CLAUDY_OPEN;

  switch (state) {
    case STATE_IDLE:
      dy = (int)(sinf((float)t / 1500.0f * 2.0f * (float)PI) * 2.0f);
      if ((t % 3500) < 150) grid = CLAUDY_BLINK;
      break;
    case STATE_THINKING:
      dy = (int)(sinf((float)t / 700.0f * 2.0f * (float)PI) * 1.5f);
      grid = CLAUDY_SQUINT;
      body = rgb565_to_lv(COL_ACCENT_THINK);
      break;
    case STATE_WORKING:
      dy = (int)(sinf((float)t / 250.0f * 2.0f * (float)PI) * 1.5f);
      dx = (int)(sinf((float)t / 500.0f * 2.0f * (float)PI) * 1.0f);
      body = rgb565_to_lv(COL_ACCENT_WORK);
      break;
    case STATE_WAITING:
      dy = (int)(fabsf(sinf((float)t / 900.0f * 2.0f * (float)PI)) * -3.0f);
      body = rgb565_to_lv(COL_ACCENT_WAIT);
      break;
    case STATE_ERROR:
      dx = (int)(sinf((float)t / 50.0f * 2.0f * (float)PI) * 4.0f);
      grid = CLAUDY_X_EYES;
      body = rgb565_to_lv(COL_ACCENT_ERROR);
      break;
    case STATE_DONE:
      dy = (int)(-fabsf(sinf((float)t / 350.0f * 2.0f * (float)PI)) * 5.0f);
      body = rgb565_to_lv(COL_ACCENT_DONE);
      if ((t % 700) < 100) grid = CLAUDY_BLINK;
      break;
    case STATE_BOOT:
      body = rgb565_to_lv(COL_CLAUDE_DIM);
      break;
  }

  draw_claudy(canvas, cx, cy, px, grid, body, eye, dx, dy);
  lv_obj_invalidate(canvas);
}
