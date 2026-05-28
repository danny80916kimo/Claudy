#include <Arduino.h>
#include "src/hw/i2c_bus.h"

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== Claudy-C6 boot ===");
  i2c_bus_begin();
  delay(50);
  i2c_scan_print();
}

void loop() {
  delay(10000);
  i2c_scan_print();
}
