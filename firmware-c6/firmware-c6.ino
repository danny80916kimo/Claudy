#include <Arduino.h>
#include "lvgl.h"
#include "src/hw/i2c_bus.h"
#include "src/hw/pmic_axp2101.h"
#include "src/hw/co5300.h"
#include "src/hw/pins.h"
#include "src/ui/lvgl_port.h"
#include "src/ui/theme.h"

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

  // Build a hello screen
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_make(0, 0, 0), 0);

  lv_obj_t *label = lv_label_create(scr);
  lv_label_set_text(label, "Hello, Claudy");
  lv_obj_set_style_text_color(label, lv_color_make(0xFF, 0xFF, 0xFF), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
  lv_obj_center(label);

  Serial.printf("Free heap after UI init: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  uint32_t delay_ms = lvgl_port_tick();
  delay(delay_ms);
}
