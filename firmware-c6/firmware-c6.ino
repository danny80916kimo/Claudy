#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(2000);   // let CDC enumerate
  Serial.println("\n=== Claudy-C6 boot ===");
  Serial.println("Skeleton sketch OK");
}

void loop() {
  delay(5000);
  Serial.println("alive");
}
