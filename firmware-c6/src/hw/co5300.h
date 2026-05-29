#pragma once
#include <Arduino.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// Initialize the CO5300 over QSPI (via the SH8601-compatible driver).
// Must be called AFTER pmic_init() (needs ALDO3 / LCD rails up).
// Returns true on success; populates panel handles in the OUT params.
bool co5300_init(esp_lcd_panel_io_handle_t *io_out,
                 esp_lcd_panel_handle_t *panel_out);

// Set AMOLED brightness 0..255 via DCS 0x51.
void co5300_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t level);
