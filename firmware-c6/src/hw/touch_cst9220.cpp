#include "touch_cst9220.h"
#include "i2c_bus.h"
#include "pins.h"
#include <Wire.h>

namespace {
volatile bool s_int_flag = false;

void IRAM_ATTR touch_isr() {
  s_int_flag = true;
}
}  // namespace

bool touch_init() {
  // Reset pulse
  pinMode(PIN_TOUCH_RST, OUTPUT);
  digitalWrite(PIN_TOUCH_RST, 1); delay(50);
  digitalWrite(PIN_TOUCH_RST, 0); delay(50);
  digitalWrite(PIN_TOUCH_RST, 1); delay(200);

  if (!i2c_probe(TOUCH_I2C_ADDR)) {
    Serial.println("touch: CST9220 not found at 0x5A");
    return false;
  }

  pinMode(PIN_TOUCH_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), touch_isr, FALLING);

  Serial.println("touch: CST9220 ready");
  return true;
}

bool touch_read(uint16_t *x, uint16_t *y) {
  if (!s_int_flag) return false;
  s_int_flag = false;

  // Write 2-byte register pointer 0xD000, then read 10 bytes.
  uint8_t out[2] = { 0xD0, 0x00 };
  uint8_t data[10] = { 0 };

  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(out, 2);
  if (Wire.endTransmission(false) != 0) return false;

  Wire.requestFrom((int)TOUCH_I2C_ADDR, 10);
  size_t i = 0;
  while (Wire.available() && i < 10) data[i++] = Wire.read();
  if (i < 10) return false;

  if (data[6] != 0xAB) return false;          // magic byte check
  uint8_t points = data[5] & 0x7F;
  if (points == 0) return false;
  uint8_t status = data[0] & 0x0F;
  if (status != 0x06) return false;           // not "press" event

  uint16_t ry = ((uint16_t)data[1] << 4) | (data[3] >> 4);
  uint16_t rx = ((uint16_t)data[2] << 4) | (data[3] & 0x0F);
  // X axis is mirrored vs display per Waveshare reference.
  *x = LCD_H_RES - rx;
  *y = ry;
  return true;
}
