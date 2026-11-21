#pragma once
#include <stdint.h>

// 只放非敏感参数。MQTT 账号密码在 secrets.h。
namespace mqtt_cfg {

// ---------- Broker ----------
constexpr const char* BROKER_HOST = "broker.emqx.io";
constexpr uint16_t BROKER_PORT = 1883;

// ---------- 连接参数 ----------
// broker 在约 1.5 倍 keepalive 内收不到任何包，就判定设备离线并发出遗嘱。
// 它直接决定验收标准 4 的离线判定时间，所以要小于一个心跳周期。
constexpr uint16_t KEEPALIVE_S = 5;

// connect() 是阻塞调用，这个值限制它等待 broker 应答的时间。
// PubSubClient 默认 15 s，会让 loop() 卡太久。
constexpr uint16_t SOCKET_TIMEOUT_S = 3;

// PubSubClient 默认 256 字节。topic 加 payload 超过它时，publish() 只会返回 false。
constexpr uint16_t BUFFER_SIZE = 512;

// ---------- 重连退避 ----------
// 失败后的等待时间从 1 s 起，每次翻倍，最大 15 s。
constexpr uint32_t RECONNECT_BACKOFF_INIT_MS = 1000;
constexpr uint32_t RECONNECT_BACKOFF_MAX_MS = 15000;
// 在退避时间上再加 0 到这个值的随机抖动，避免 broker 重启后多块板同时重连。
constexpr uint32_t RECONNECT_JITTER_MS = 500;

// ---------- 发布 ----------
constexpr uint32_t PUBLISH_INTERVAL_MS = 2000;

// ---------- Topic ----------
// 两层结构：
//   gw  = 网关，即这块 ESP32 本身。gatewayId 运行时由 MAC 生成，如 esp32-a1b2c3d4e5f6。
//   dev = 3D 场景里的设备。deviceId 是固定约定（tank-01、fan-01 ...），见 scene-contract.json。
// 每个 topic 一条 snprintf 格式串：第一个 %s 填 TOPIC_PREFIX，第二个 %s 填 gatewayId 或 deviceId。
// 加新 topic 就在这里加一行，拼接逻辑不用动。暂定，协议 V1 文档写好后以文档为准。
//
// 前缀是公共 broker 上的命名空间，要带自己项目独有的部分，避免和别人撞。
constexpr const char* TOPIC_PREFIX = "twin-plant-zfj";

constexpr const char* TOPIC_GW_STATUS_FMT = "%s/gw/%s/status";  // 网关在线/离线，retain，遗嘱用这个
constexpr const char* TOPIC_GW_DATA_FMT = "%s/gw/%s/data";      // 网关自己的数据（运行时间、剩余堆）
constexpr const char* TOPIC_DEV_DATA_FMT = "%s/dev/%s/data";    // 设备数据，接上 UART 后才有

// 拼接后的 topic 缓冲区大小（含结尾 \0）。
constexpr uint8_t TOPIC_MAX_LEN = 96;

// ---------- 协议 ----------
constexpr uint8_t PROTOCOL_VERSION = 1;

}  // namespace mqtt_cfg
