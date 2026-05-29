#include <Arduino.h>
#include "src/hw/i2c_bus.h"
#include "src/hw/pmic_axp2101.h"
#include "src/hw/co5300.h"
#include "src/hw/pins.h"
#include "esp_heap_caps.h"

esp_lcd_panel_io_handle_t g_io   = nullptr;
esp_lcd_panel_handle_t    g_panel = nullptr;

// CO5300 only accepts even-aligned draw windows (2-px granularity). Drawing
// 1-px-tall rows produced 1-row windows the panel rejected -> black screen.
// Draw even-height bands instead (480 is divisible by 16).
static const int BAND_ROWS = 16;
static uint16_t *band_buf = nullptr;   // 480 × BAND_ROWS × 2 bytes

static void fill_color(uint16_t color565) {
  for (int i = 0; i < LCD_H_RES * BAND_ROWS; i++) band_buf[i] = color565;
  for (int y = 0; y < LCD_V_RES; y += BAND_ROWS) {
    esp_lcd_panel_draw_bitmap(g_panel, 0, y, LCD_H_RES, y + BAND_ROWS, band_buf);
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  if (!pmic_init()) { Serial.println("PMIC fail"); while (true) delay(1000); }
  delay(100);

  if (!co5300_init(&g_io, &g_panel)) { Serial.println("CO5300 fail"); while (true) delay(1000); }
  esp_lcd_panel_disp_on_off(g_panel, true);
  co5300_set_brightness(g_io, 200);

  band_buf = (uint16_t*) heap_caps_malloc(LCD_H_RES * BAND_ROWS * 2, MALLOC_CAP_DMA);
  if (!band_buf) { Serial.println("band_buf alloc fail"); while (true) delay(1000); }

  Serial.printf("Free heap after init: %u bytes\n", ESP.getFreeHeap());
}

void loop() {
  Serial.println("RED");   fill_color(0xF800); delay(1500);
  Serial.println("GREEN"); fill_color(0x07E0); delay(1500);
  Serial.println("BLUE");  fill_color(0x001F); delay(1500);
  Serial.println("WHITE"); fill_color(0xFFFF); delay(1500);
}
