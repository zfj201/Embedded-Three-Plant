#pragma once

// 网络名（SSID）
constexpr const char* WIFI_SSID = "your ssid";
// WPA2-Enterprise（802.1X）的账号和密码。普通 WPA2 网络不需要账号，见 wifi_manager.cpp。
constexpr const char* WIFI_USERNAME = "your username";
constexpr const char* WIFI_PASSWORD = "your password";


// 公共 broker 不需要账号。留空字符串表示不带用户名/密码连接。
constexpr const char* MQTT_USER = "";
constexpr const char* MQTT_PASSWORD = "";
