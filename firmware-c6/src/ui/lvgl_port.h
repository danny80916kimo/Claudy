#pragma once
#include <Arduino.h>
#include "lvgl.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Initialize LVGL and bind it to the already-initialized CO5300 panel.
// Allocates two partial draw buffers in DRAM.
bool lvgl_port_init(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel);

// Drive from loop(). Calls lv_timer_handler. Returns the ms to wait until
// the next call (use as a delay).
uint32_t lvgl_port_tick();

// Mutex around LVGL state — required because the HTTP handler (web server
// task on core 0) and the main loop both touch widgets.
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock();
