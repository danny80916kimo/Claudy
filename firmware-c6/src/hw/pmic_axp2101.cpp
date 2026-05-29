#include "pmic_axp2101.h"
#include "i2c_bus.h"
#include "pins.h"

namespace {
constexpr uint8_t ADDR = I2C_ADDR_AXP2101;

// Selected AXP2101 registers we touch.
// (Confirmed against XPowersLib AXP2101Constants.h and Waveshare axp2101_bsp.cpp.)
constexpr uint8_t REG_LDO_ONOFF_CFG0 = 0x90;  // LDO_ONOFF_CTRL0: ALDO1..4, BLDO1..2
constexpr uint8_t REG_LDO_ONOFF_CFG1 = 0x91;  // LDO_ONOFF_CTRL1: DLDO1..2
constexpr uint8_t REG_ALDO1_VOLT     = 0x92;  // ALDO1 voltage (LDO_VOL0)
constexpr uint8_t REG_ALDO2_VOLT     = 0x93;  // ALDO2 voltage (LDO_VOL1)
constexpr uint8_t REG_ALDO3_VOLT     = 0x94;  // ALDO3 voltage (LDO_VOL2) -> LCD VCI
constexpr uint8_t REG_ALDO4_VOLT     = 0x95;  // ALDO4 voltage (LDO_VOL3)
constexpr uint8_t REG_BLDO1_VOLT     = 0x96;  // BLDO1 voltage (LDO_VOL4)
constexpr uint8_t REG_VBAT_H         = 0x34;  // 14-bit ADC, high byte
constexpr uint8_t REG_VBAT_L         = 0x35;

// LDO_ONOFF_CTRL0 bits: 0=ALDO1, 1=ALDO2, 2=ALDO3, 3=ALDO4, 4=BLDO1, 5=BLDO2

// Voltage encoding for ALDO/BLDO rails: voltage_mV = 500 + 100 * regval.
// 3300 mV -> regval 28.  1800 mV -> regval 13.
constexpr uint8_t VOLT_3V3 = 28;
constexpr uint8_t VOLT_1V8 = 13;
}  // namespace

bool pmic_init() {
  if (!i2c_probe(ADDR)) {
    Serial.println("AXP2101: not detected at 0x34");
    return false;
  }

  // Set ALDO rails to 3.3V (matches Waveshare reference: enables LCD VCI/logic).
  // ALDO3 is the rail toggled by the Waveshare DisplayPort_DispReset() sequence.
  i2c_write_reg(ADDR, REG_ALDO1_VOLT, VOLT_3V3);
  i2c_write_reg(ADDR, REG_ALDO2_VOLT, VOLT_3V3);
  i2c_write_reg(ADDR, REG_ALDO3_VOLT, VOLT_3V3);
  i2c_write_reg(ADDR, REG_ALDO4_VOLT, VOLT_3V3);
  // BLDO1 = 1.8V (LCD AVDD / analog).  Harmless if unused on this board variant.
  i2c_write_reg(ADDR, REG_BLDO1_VOLT, VOLT_1V8);

  // Enable ALDO1..ALDO4 (bits 0..3) and BLDO1 (bit 4) via LDO_ONOFF_CFG0.
  uint8_t cfg0 = i2c_read_reg(ADDR, REG_LDO_ONOFF_CFG0);
  cfg0 |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
  i2c_write_reg(ADDR, REG_LDO_ONOFF_CFG0, cfg0);

  delay(50);   // let rails settle

  Serial.println("AXP2101: LCD power rails enabled (ALDO1..4=3V3, BLDO1=1V8)");
  return true;
}

void pmic_set_aldo3(bool on) {
  uint8_t cfg0 = i2c_read_reg(ADDR, REG_LDO_ONOFF_CFG0);
  if (on) cfg0 |=  (1 << 2);
  else    cfg0 &= ~(1 << 2);
  i2c_write_reg(ADDR, REG_LDO_ONOFF_CFG0, cfg0);
}

void pmic_set_brightness(uint8_t level) {
  // CO5300 brightness is driven via DSI brightness command; here we keep
  // AXP at fixed 3.3V/1.8V and route brightness through the LCD command.
  // For now this is a no-op placeholder so the API exists.
  (void)level;
}

uint16_t pmic_battery_mv() {
  uint8_t h = i2c_read_reg(ADDR, REG_VBAT_H);
  uint8_t l = i2c_read_reg(ADDR, REG_VBAT_L);
  uint16_t raw = ((uint16_t)h << 8) | l;
  // 14-bit ADC, ~1mV/LSB on this board.
  return raw & 0x3FFF;
}
