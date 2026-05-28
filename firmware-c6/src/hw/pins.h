#pragma once

// === LCD (CO5300 AMOLED, QSPI) ===
#define PIN_LCD_CS      15
#define PIN_LCD_SCLK    0
#define PIN_LCD_D0      1
#define PIN_LCD_D1      2
#define PIN_LCD_D2      3
#define PIN_LCD_D3      4
// LCD_RST is not on a GPIO — handled by AXP2101 power sequencing.

// === Touch (CST9220, I2C addr 0x5A) ===
#define PIN_TOUCH_RST   11
#define PIN_TOUCH_INT   5
#define TOUCH_I2C_ADDR  0x5A

// === Shared I2C bus (touch + AXP2101 + QMI8658 + PCF85063) ===
#define PIN_I2C_SCL     7
#define PIN_I2C_SDA     8
#define I2C_FREQ_HZ     400000

// === I2C device addresses ===
#define I2C_ADDR_AXP2101    0x34
#define I2C_ADDR_QMI8658    0x6B
#define I2C_ADDR_PCF85063   0x51
#define I2C_ADDR_CST9220    0x5A

// === LCD spec ===
#define LCD_H_RES       480
#define LCD_V_RES       480
#define LCD_BITS_PP     16   // RGB565
