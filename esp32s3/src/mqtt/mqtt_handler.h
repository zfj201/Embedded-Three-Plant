#pragma once
#include <stdint.h>
void mqtt_handler_on_message(char* topic, uint8_t* payload, unsigned int length);