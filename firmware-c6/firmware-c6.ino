#include <Arduino.h>
#include "src/hw/i2c_bus.h"
#include "src/hw/pmic_axp2101.h"

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");

  i2c_bus_begin();
  delay(50);

  Serial.println("--- Scan #1 (before PMIC init) ---");
  i2c_scan_print();

  if (!pmic_init()) {
    Serial.println("FATAL: AXP2101 init failed; halting");
    while (true) delay(1000);
  }

  delay(100);
  Serial.println("--- Scan #2 (after PMIC init) ---");
  i2c_scan_print();

  Serial.printf("Battery: %u mV\n", pmic_battery_mv());
}

void loop() {
  delay(10000);
}
