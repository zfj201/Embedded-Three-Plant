#include <Arduino.h>
#include <WiFi.h>
#include "network/wifi_manager.h"
#include "mqtt/mqtt_manager.h"

uint32_t last_print_time = 0;

void setup() {
  Serial.begin(115200);
  wifi_manager_init();
  mqtt_manager_init();
  // last_print_time = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  // Serial.println("Hello, World!");
  // delay(1000);
  wifi_manager_update();
  mqtt_manager_update();
    // if(millis() - last_print_time > 1000) {
    //   last_print_time = millis();
    //   Serial.println(wifi_manager_is_connected());
    // }
  
}

