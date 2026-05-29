#pragma once
#include <Arduino.h>

bool touch_init();

// Non-blocking read. Returns true if a fresh touch is available;
// outputs coordinates in display pixel space (0..LCD_W, 0..LCD_H).
bool touch_read(uint16_t *x, uint16_t *y);
