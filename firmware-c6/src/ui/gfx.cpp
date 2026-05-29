#include "gfx.h"

void gfx_fill_bg(lv_obj_t *canvas, lv_color_t c) {
  lv_canvas_fill_bg(canvas, c, LV_OPA_COVER);
}

void gfx_fill_rect(lv_obj_t *canvas, int x, int y, int w, int h, lv_color_t c) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = c;
  dsc.bg_opa   = LV_OPA_COVER;
  dsc.border_width = 0;
  dsc.radius   = 0;
  lv_canvas_draw_rect(canvas, x, y, w, h, &dsc);
}

void gfx_fill_circle(lv_obj_t *canvas, int cx, int cy, int r, lv_color_t c) {
  // Implement as a filled rect with radius = LV_RADIUS_CIRCLE on an equal-side bbox.
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_color = c;
  dsc.bg_opa   = LV_OPA_COVER;
  dsc.border_width = 0;
  dsc.radius   = LV_RADIUS_CIRCLE;
  lv_canvas_draw_rect(canvas, cx - r, cy - r, r * 2, r * 2, &dsc);
}
