#pragma once

void wifi_manager_init(void);   // setup() 里调用一次
void wifi_manager_update(void); // loop() 每圈调用一次，不阻塞
bool wifi_manager_is_connected(void);  // 给 MQTT 层查询用，现在可以先不实现