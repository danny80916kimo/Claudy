#pragma once
#include <Arduino.h>

// AXP2101 PMIC controls all on-board power rails on this board.
// Must be initialized before any LCD or touch I/O.
bool pmic_init();

// AMOLED brightness 0..100. Stub for now — real brightness is driven by the
// CO5300 via DCS 0x51 (see co5300_set_brightness in Task 4). This entry point
// exists so callers can stay PMIC-flavored if a future variant routes
// brightness through AXP instead.
void pmic_set_brightness(uint8_t level);

// Returns battery voltage in mV (0 if no battery / not detected).
uint16_t pmic_battery_mv();
