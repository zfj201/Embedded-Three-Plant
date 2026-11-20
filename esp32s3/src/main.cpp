#include <Arduino.h>

#include "network/wifi_manager.h"

void setup() {
  Serial.begin(115200);
  wifi_manager_init();
}

void loop() {
  // put your main code here, to run repeatedly:
  // Serial.println("Hello, World!");
  // delay(1000);
  wifi_manager_update();
}

