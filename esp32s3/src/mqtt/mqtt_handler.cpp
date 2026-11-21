#include <Arduino.h>
#include "mqtt_handler.h"

void mqtt_handler_on_message(char* topic, uint8_t* payload, unsigned int length)
{
    Serial.printf("MQTT RX topic=%s len=%u payload=%.*s\n", topic, length, (int)length, (const char*)payload);
}