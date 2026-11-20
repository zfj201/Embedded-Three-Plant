#pragma once
#include <stdint.h>

// 只放非敏感参数。SSID 和密码在 secrets.h。
namespace wifi_cfg {

// 单次连接尝试的最长等待时间，超时就放弃这一次，进入退避。
constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;

// 失败后的等待时间从 1 s 起，每次翻倍，最大 10 s。
// 上限不能设大：验收标准 2 要求断网恢复后 30 s 内重连。
constexpr uint32_t BACKOFF_INIT_MS = 1000;
constexpr uint32_t BACKOFF_MAX_MS = 10000;

// 连续 3 次认证类失败，就判定为密码错误。
// 判定后改为每 30 s 重试一次，避免每秒重试刷屏。
constexpr uint8_t AUTH_FAIL_LIMIT = 3;
constexpr uint32_t AUTH_FAIL_RETRY_MS = 30000;

// 路由器后台看到的设备名。
constexpr const char* HOSTNAME = "twinplant-esp32s3";

}  // namespace wifi_cfg
