#include "co5300.h"
#include "pins.h"
#include "pmic_axp2101.h"
#include "co5300_vendor/esp_lcd_sh8601.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define LCD_HOST  SPI2_HOST

// CO5300 init command array — copied verbatim from Waveshare's
// 08_LVGL_V8_Test/bsp_lvgl_port.cpp (lcd_init_cmds[]). Panel-specific magic;
// do not edit. 0x2A/0x2B set the 0..479 column/row window (0x01DF = 479).
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
  { 0x11, (uint8_t[]){ 0x00 }, 0, 600 },
  { 0xFE, (uint8_t[]){ 0x20 }, 1, 0 },
  { 0x19, (uint8_t[]){ 0x10 }, 1, 0 },
  { 0x1C, (uint8_t[]){ 0xA0 }, 1, 0 },
  { 0xFE, (uint8_t[]){ 0x00 }, 1, 0 },
  { 0xC4, (uint8_t[]){ 0x80 }, 1, 0 },
  { 0x3A, (uint8_t[]){ 0x55 }, 1, 0 },
  { 0x35, (uint8_t[]){ 0x00 }, 1, 0 },
  { 0x36, (uint8_t[]){ 0x30 }, 1, 0 },
  { 0x53, (uint8_t[]){ 0x20 }, 1, 0 },
  { 0x51, (uint8_t[]){ 0xFF }, 1, 0 },
  { 0x63, (uint8_t[]){ 0xFF }, 1, 0 },
  { 0x2A, (uint8_t[]){ 0x00, 0x00, 0x01, 0xDF }, 4, 0 },
  { 0x2B, (uint8_t[]){ 0x00, 0x00, 0x01, 0xDF }, 4, 0 },
  { 0x29, (uint8_t[]){ 0x00 }, 0, 100 },
};

static void co5300_reset_pulse() {
  // Waveshare DisplayPort_DispReset(): ALDO3 1 -> 0 -> 1, 100ms each.
  pmic_set_aldo3(true);  delay(100);
  pmic_set_aldo3(false); delay(100);
  pmic_set_aldo3(true);  delay(100);
}

bool co5300_init(esp_lcd_panel_io_handle_t *io_out,
                 esp_lcd_panel_handle_t *panel_out) {
  spi_bus_config_t buscfg = {};
  buscfg.sclk_io_num = PIN_LCD_SCLK;
  buscfg.data0_io_num = PIN_LCD_D0;
  buscfg.data1_io_num = PIN_LCD_D1;
  buscfg.data2_io_num = PIN_LCD_D2;
  buscfg.data3_io_num = PIN_LCD_D3;
  buscfg.max_transfer_sz = LCD_H_RES * LCD_V_RES * LCD_BITS_PP / 8;
  if (spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO) != ESP_OK) {
    Serial.println("CO5300: spi_bus_initialize failed");
    return false;
  }

  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_io_spi_config_t io_config = {};
  io_config.cs_gpio_num = PIN_LCD_CS;
  io_config.dc_gpio_num = -1;
  io_config.spi_mode = 0;
  io_config.pclk_hz = 40 * 1000 * 1000;
  io_config.trans_queue_depth = 2;
  io_config.lcd_cmd_bits = 32;
  io_config.lcd_param_bits = 8;
  io_config.flags.quad_mode = true;
  if (esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                               &io_config, &io) != ESP_OK) {
    Serial.println("CO5300: new_panel_io_spi failed");
    return false;
  }

  sh8601_vendor_config_t vendor_config = {};
  vendor_config.init_cmds = lcd_init_cmds;
  vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);
  vendor_config.flags.use_qspi_interface = 1;

  esp_lcd_panel_handle_t panel = nullptr;
  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = -1;   // no GPIO reset; ALDO3 handles it
  panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panel_config.bits_per_pixel = LCD_BITS_PP;
  panel_config.vendor_config = &vendor_config;
  if (esp_lcd_new_panel_sh8601(io, &panel_config, &panel) != ESP_OK) {
    Serial.println("CO5300: new_panel_sh8601 failed");
    return false;
  }

  co5300_reset_pulse();

  if (esp_lcd_panel_init(panel) != ESP_OK) {
    Serial.println("CO5300: panel_init failed");
    return false;
  }

  *io_out = io;
  *panel_out = panel;
  Serial.println("CO5300: init OK");
  return true;
}

void co5300_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t level) {
  // DCS 0x51 = Write Display Brightness.
  uint8_t param = level;
  esp_lcd_panel_io_tx_param(io, 0x51, &param, 1);
}
