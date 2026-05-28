#pragma once
#include <Arduino.h>

void i2c_bus_begin();
bool i2c_probe(uint8_t addr);
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg);
void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
void i2c_scan_print();
