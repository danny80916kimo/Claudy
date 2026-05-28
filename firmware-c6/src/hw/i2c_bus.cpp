#include "i2c_bus.h"
#include "pins.h"
#include <Wire.h>

void i2c_bus_begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
}

bool i2c_probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

uint8_t i2c_read_reg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((int)addr, 1);
  return Wire.available() ? Wire.read() : 0;
}

void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void i2c_scan_print() {
  Serial.println("I2C scan:");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (i2c_probe(a)) {
      Serial.printf("  found device at 0x%02X\n", a);
      n++;
    }
  }
  Serial.printf("I2C scan: %d device(s) found\n", n);
}
