#pragma once
#include <Arduino.h>

// AXP2101 PMIC controls all on-board power rails on this board.
// Must be initialized before any LCD or touch I/O.
bool pmic_init();

// AMOLED brightness 0..100. Implemented as a register write (HVbrightness reg).
void pmic_set_brightness(uint8_t level);

// Returns battery voltage in mV (0 if no battery / not detected).
uint16_t pmic_battery_mv();
