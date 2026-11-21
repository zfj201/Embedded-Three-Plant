#pragma once

void mqtt_manager_init(void);   // setup() 里调用一次
void mqtt_manager_update(void); // loop() 每圈调用一次，不阻塞
bool mqtt_manager_is_connected(void);  // 给 MQTT 层查询用