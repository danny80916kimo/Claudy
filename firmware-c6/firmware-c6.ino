#include <Arduino.h>
#include "lvgl.h"
#include "src/hw/i2c_bus.h"
#include "src/hw/pmic_axp2101.h"
#include "src/hw/co5300.h"
#include "src/hw/pins.h"
#include "src/hw/touch_cst9220.h"
#include "src/ui/lvgl_port.h"
#include "src/ui/theme.h"
#include "src/ui/mascot.h"

esp_lcd_panel_io_handle_t g_io;
esp_lcd_panel_handle_t    g_panel;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  if (!pmic_init()) while (true) delay(1000);
  delay(100);

  if (!co5300_init(&g_io, &g_panel)) while (true) delay(1000);
  esp_lcd_panel_disp_on_off(g_panel, true);
  co5300_set_brightness(g_io, 200);

  if (!lvgl_port_init(g_io, g_panel)) while (true) delay(1000);

  if (!touch_init()) {
    Serial.println("WARN: touch init failed (continuing without touch)");
  }

  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 0), 0);

  lv_obj_t *mascot = mascot_create(scr);
  if (!mascot) {
    Serial.println("FATAL: mascot canvas alloc failed; shrink MASCOT_W/H in theme.h");
    while (true) delay(1000);
  }

  static MascotState anim_state = STATE_IDLE;
  static lv_timer_t *mascot_timer = lv_timer_create(
    [](lv_timer_t *t) {
      lv_obj_t *c = (lv_obj_t*) t->user_data;
      mascot_draw(c, anim_state);
    }, mascot_anim_interval(STATE_IDLE), mascot);
  (void)mascot_timer;

  Serial.printf("Free heap after mascot: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  uint16_t tx, ty;
  if (touch_read(&tx, &ty)) {
    Serial.printf("touch: x=%u y=%u\n", tx, ty);
  }
  uint32_t delay_ms = lvgl_port_tick();
  delay(delay_ms);
}
