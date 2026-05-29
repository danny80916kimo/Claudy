#include "lvgl_port.h"
#include "../hw/pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t    s_panel;
static lv_disp_draw_buf_t        s_draw_buf;
static lv_color_t               *s_buf1 = nullptr;
static lv_color_t               *s_buf2 = nullptr;
static SemaphoreHandle_t         s_mutex = nullptr;

static constexpr size_t LVGL_BUF_LINES = 48;
static constexpr size_t LVGL_BUF_PX    = LCD_H_RES * LVGL_BUF_LINES;

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
  esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                            area->x2 + 1, area->y2 + 1, color_map);
  lv_disp_flush_ready(drv);
}

// CONFIRMED ON HARDWARE (Task 4 / Gate G3): the CO5300 only accepts
// even-aligned draw windows (2-px granularity). Odd-aligned areas are
// silently dropped -> black screen. Snap every flush area to even bounds,
// exactly like Waveshare's my_disp_rounder. WITHOUT THIS, LVGL partial
// draws will not appear.
static void rounder_cb(lv_disp_drv_t *drv, lv_area_t *area) {
  area->x1 = (area->x1 >> 1) << 1;          // start down to even
  area->y1 = (area->y1 >> 1) << 1;
  area->x2 = ((area->x2 >> 1) << 1) + 1;    // end up to odd -> even width/height
  area->y2 = ((area->y2 >> 1) << 1) + 1;
}

bool lvgl_port_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel) {
  s_io = io;
  s_panel = panel;
  s_mutex = xSemaphoreCreateRecursiveMutex();
  if (!s_mutex) { Serial.println("lvgl_port: mutex alloc fail"); return false; }

  lv_init();

  s_buf1 = (lv_color_t*) heap_caps_aligned_alloc(4, LVGL_BUF_PX * sizeof(lv_color_t),
                                                 MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  s_buf2 = (lv_color_t*) heap_caps_aligned_alloc(4, LVGL_BUF_PX * sizeof(lv_color_t),
                                                 MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!s_buf1 || !s_buf2) {
    Serial.printf("lvgl_port: draw buf alloc fail (need 2x %u bytes)\n",
                  (unsigned)(LVGL_BUF_PX * sizeof(lv_color_t)));
    return false;
  }
  lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, LVGL_BUF_PX);

  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = LCD_H_RES;
  drv.ver_res = LCD_V_RES;
  drv.flush_cb = flush_cb;
  drv.rounder_cb = rounder_cb;   // REQUIRED: CO5300 needs even-aligned windows (see G3)
  drv.draw_buf = &s_draw_buf;
  lv_disp_drv_register(&drv);

  Serial.printf("lvgl_port: ready (2x%u byte draw buffers in DMA-capable DRAM)\n",
                (unsigned)(LVGL_BUF_PX * sizeof(lv_color_t)));
  return true;
}

uint32_t lvgl_port_tick() {
  if (!lvgl_port_lock(0)) return 5;
  uint32_t next = lv_timer_handler();
  lvgl_port_unlock();
  if (next > 500) next = 500;
  if (next < 5)   next = 5;
  return next;
}

bool lvgl_port_lock(int timeout_ms) {
  if (!s_mutex) return false;
  return xSemaphoreTakeRecursive(s_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void lvgl_port_unlock() {
  if (s_mutex) xSemaphoreGiveRecursive(s_mutex);
}
