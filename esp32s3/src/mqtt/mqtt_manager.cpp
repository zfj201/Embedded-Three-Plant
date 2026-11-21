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
// status 消息，init() 里拼一次。offline 是连接时交给 broker 的遗嘱，之后改不了，
// 所以不带 timestamp、sequence 这类会随时间变旧的字段。
static char payload_status_online[128];
static char payload_status_offline[128];

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
    // 连接的同时设立遗嘱消息。
    bool success = mqttClient.connect(gateway_id,topic_gw_status,1,true,payload_status_offline);
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

    // 4. 生成 status 消息（online / offline）
    snprintf(
        payload_status_online,
        sizeof(payload_status_online),
        "{\"ver\":%u,\"gatewayId\":\"%s\",\"type\":\"status\",\"value\":\"online\"}",
        (unsigned)mqtt_cfg::PROTOCOL_VERSION,
        gateway_id);

    snprintf(
        payload_status_offline,
        sizeof(payload_status_offline),
        "{\"ver\":%u,\"gatewayId\":\"%s\",\"type\":\"status\",\"value\":\"offline\"}",
        (unsigned)mqtt_cfg::PROTOCOL_VERSION,
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
            // 发送上线消息。
            mqtt_result_t publish_res = mqtt_publish(topic_gw_status, payload_status_online, true);
            if(publish_res != MQTT_RESULT_OK) {
                Serial.println("Warn! MQTT online message published falied");
                schedule_retry();
            }
            else
            {
                backoff_time = mqtt_cfg::RECONNECT_BACKOFF_INIT_MS;
                retry_wait_time = 0;
            }
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
    // 不能只用current state来判断，因为有可能状态机没来得及切换，但实际已经断开连接了。
    return current_state == MQTTState::CONNECTED && mqttClient.connected();
}

mqtt_result_t mqtt_publish(const char* topic, const char* payload, bool retained)
{
    // 1. 检查连接状态
    if (current_state != MQTTState::CONNECTED ||
        !mqttClient.connected()) {

        return MQTT_RESULT_NOT_CONNECTED;
    }

    // 2. 检查参数
    if (topic == nullptr || payload == nullptr ||
        topic[0] == '\0') {

        return MQTT_RESULT_PUBLISH_FAILED;
    }

    // 3. 计算 Topic 和 Payload 长度
    const size_t topic_len = strlen(topic);
    const size_t payload_len = strlen(payload);

    // MQTT 最大固定报头预留 5 字节
    constexpr size_t HEADER_SIZE = 5;
    constexpr size_t TOPIC_LENGTH_SIZE = 2;

    const size_t required_size =
        HEADER_SIZE +
        TOPIC_LENGTH_SIZE +
        topic_len +
        payload_len;

    // 4. 检查缓冲区容量
    if (required_size > mqtt_cfg::BUFFER_SIZE) {
        return MQTT_RESULT_PAYLOAD_TOO_LARGE;
    }

    // 5. 执行发布
    const bool success = mqttClient.publish(
        topic,
        payload,
        retained
    );

    if (success) {
        return MQTT_RESULT_OK;
    }

    return MQTT_RESULT_PUBLISH_FAILED;
}

const char* mqtt_get_data_topic()
{
    return topic_gw_data;
}
const char* mqtt_get_gateway_id()
{
    return gateway_id;
}