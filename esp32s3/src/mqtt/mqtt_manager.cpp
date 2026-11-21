#include <Arduino.h>
#include "mqtt_manager.h"
#include "../config/mqtt_config.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include "../network/wifi_manager.h"
#include <esp_mac.h>

// ==================== 状态定义 ====================
// 不设置连接中状态，因为PubSubClient 的 connect() 会一直阻塞到成功或超时，所以 CONNECTING 只存在于一次 update() 调用内部
enum class MQTTState
{
    DISCONNECTED,
    CONNECTED,
    WAIT_RETRY
};

// ==================== 内部变量 ====================
static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static MQTTState current_state = MQTTState::DISCONNECTED;
// 当前状态进入时的时间
static uint32_t state_enter_time = 0;
// 当前退避时间
static uint32_t backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
// 重试需要等待的时间
static uint32_t retry_wait_time = 0;

// 网关 ID，由芯片 MAC 生成，如 "esp32-a1b2c3d4e5f6"（18 个字符加结尾符）。同时用作 MQTT 的 clientId。
static char gateway_id[24];
// 完整 topic，init() 里用 mqtt_cfg 的格式串拼一次，之后一直用。
static char topic_gw_status[mqtt_cfg::TOPIC_MAX_LEN];
static char topic_gw_data[mqtt_cfg::TOPIC_MAX_LEN];

// ==================== 内部函数 ====================
static void set_state(MQTTState new_state)
{
    current_state = new_state;
    state_enter_time = millis();
}

static bool try_connect()
{
    Serial.println("MQTT connecting");
    const uint32_t t0 = millis();
    bool success = mqttClient.connect(gateway_id);
    uint32_t take_time = millis() - t0;
    if (success)
    {
        Serial.printf(
            "MQTT Connected! Take %lu ms\n",
            (unsigned long)take_time);
    }
    else
    {
        Serial.printf(
            "MQTT Connect Failed! State: %d, Take %lu ms\n",
            mqttClient.state(),
            (unsigned long)take_time);
    }
    return success;
}

// 连接失败，进入等待重试
static void schedule_retry()
{
    mqttClient.disconnect();

    retry_wait_time = backoff_time + random(0, mqtt_cfg::RECONNECT_JITTER_MS + 1);

    Serial.printf(
        "Retry connect after %lu ms\n",
        (unsigned long)retry_wait_time);
    backoff_time *= 2;
    if (backoff_time > mqtt_cfg::RECONNECT_BACKOFF_MAX_MS)
    {
        backoff_time = mqtt_cfg::RECONNECT_BACKOFF_MAX_MS;
    }
    set_state(MQTTState::WAIT_RETRY);
}

// ==================== 对外函数 ====================
void mqtt_manager_init(void)
{
    Serial.println("MQTT Manager Init");
    // 先生成 gateway_id，再拼两个 topic，最后调用 setServer、setKeepAlive、setSocketTimeout、setBufferSize。
    // 1. 获取 MAC 地址
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // 2. 生成 Gateway ID
    snprintf(
        gateway_id,
        sizeof(gateway_id),
        "esp32-%02x%02x%02x%02x%02x%02x",
        mac[0], mac[1], mac[2],
        mac[3], mac[4], mac[5]);

    // 3. 生成 Topic
    snprintf(
        topic_gw_status,
        sizeof(topic_gw_status),
        mqtt_cfg::TOPIC_GW_STATUS_FMT,
        mqtt_cfg::TOPIC_PREFIX,
        gateway_id);

    snprintf(
        topic_gw_data,
        sizeof(topic_gw_data),
        mqtt_cfg::TOPIC_GW_DATA_FMT,
        mqtt_cfg::TOPIC_PREFIX,
        gateway_id);

    // 初始化 MQTT Client
    mqttClient.setServer(mqtt_cfg::BROKER_HOST, mqtt_cfg::BROKER_PORT);
    mqttClient.setKeepAlive(mqtt_cfg::KEEPALIVE_S);
    mqttClient.setSocketTimeout(mqtt_cfg::SOCKET_TIMEOUT_S);
    mqttClient.setBufferSize(mqtt_cfg::BUFFER_SIZE);
}

void mqtt_manager_update(void)
{
    if (!wifi_manager_is_connected())
    {
        if (current_state != MQTTState::DISCONNECTED)
        {
            mqttClient.disconnect();
            set_state(MQTTState::DISCONNECTED);
            backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
            retry_wait_time = 0;
        }
        return;
    }
    switch (current_state)
    {
    case MQTTState::DISCONNECTED:
    {
        bool success = try_connect();
        if (success)
        {
            set_state(MQTTState::CONNECTED);
            backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
            retry_wait_time = 0;
        }
        else
        {
            schedule_retry();
        }
        break;
    }

    case MQTTState::CONNECTED:
    {
        bool res = mqttClient.loop();

        if (res)
        {
        }
        else
        {
            set_state(MQTTState::DISCONNECTED);
            Serial.println("MQTT Disconnected");
        }
        break;
    }

    case MQTTState::WAIT_RETRY:
    {
        // 问：等够了吗？        → 够了就重新发起连接
        if (millis() - state_enter_time > retry_wait_time)
        {
            set_state(MQTTState::DISCONNECTED);
        }
    }
    break;
    }
}

bool mqtt_manager_is_connected(void)
{
    return current_state == MQTTState::CONNECTED;
}