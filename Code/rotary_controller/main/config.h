/**
 * @file config.h
 * @brief 控制器配置定义
 *
 * 集中管理所有配置参数，便于维护和调整
 */

#pragma once

#include <stdint.h>

// ==================== GPIO定义 ====================
#define CONFIG_ENCODER_PIN_A           2   // 编码器A相
#define CONFIG_ENCODER_PIN_B           3   // 编码器B相
#define CONFIG_ENCODER_PIN_BTN         4   // 编码器按键
#define CONFIG_LED_INDICATOR_PIN       5   // 状态指示LED

// ==================== 编码器参数 ====================
#define CONFIG_ENCODER_STEPS           4   // 编码器每圈步数
#define CONFIG_ENCODER_MIN_VALUE       -99999
#define CONFIG_ENCODER_MAX_VALUE       99999
#define CONFIG_ENCODER_ACCELERATION    50  // 加速系数

// ==================== 按键参数 ====================
#define CONFIG_BTN_DEBOUNCE_MS         50    // 消抖时间
#define CONFIG_BTN_LONG_PRESS_MS       2000  // 长按触发时间
#define CONFIG_BTN_DOUBLE_CLICK_MS     300   // 双击检测时间

// ==================== PWM参数 ====================
#define CONFIG_PWM_RESOLUTION          9     // PWM分辨率（9位=0-511）
#define CONFIG_MAX_DUTY                511   // 最大duty值 (2^9 - 1)
#define CONFIG_MIN_DUTY                0     // 最小duty值

// ==================== 亮度和色温调节参数 ====================
#define CONFIG_BRIGHTNESS_STEP         1     // 亮度调节步进（编码器每格）
#define CONFIG_TEMPERATURE_STEP        5     // 色温调节步进（编码器每格）
#define CONFIG_MIN_BRIGHTNESS          10    // 最小亮度（避免完全关闭）
#define CONFIG_MAX_BRIGHTNESS          CONFIG_MAX_DUTY

// ==================== ESP-NOW配置 ====================
#define CONFIG_ESPNOW_WIFI_CHANNEL     1     // 默认WiFi信道
#define CONFIG_ESPNOW_MAX_CHANNEL      13    // 最大信道（中国: 1-13, 北美: 1-11）
#define CONFIG_ESPNOW_CHANNEL_SCAN_TIMEOUT_MS  500  // 每信道扫描超时
#define CONFIG_ESPNOW_MAX_RETRY        3     // 最大重试次数
#define CONFIG_ESPNOW_RETRY_DELAY_MS   50    // 重试延迟

// ==================== 配对参数 ====================
#define CONFIG_PAIRING_MAX_RETRY       3     // 配对最大重试次数（遍历所有信道）
#define CONFIG_PAIRING_RESPONSE_TIMEOUT_MS  1000  // 配对响应超时
#define CONFIG_PAIRING_BROADCAST_MAC   {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}

// ==================== 电源管理参数 ====================
#define CONFIG_SLEEP_TIMEOUT_MS        5000  // 无操作后进入睡眠时间（5秒）
#define CONFIG_SLEEP_CHECK_INTERVAL_MS 500   // 睡眠检查间隔

// ==================== GPIO唤醒配置 ====================
// 编码器A/B相或按键任意一个下降沿唤醒
#define CONFIG_WAKEUP_GPIO_MASK  ((1ULL << CONFIG_ENCODER_PIN_A) | \
                                  (1ULL << CONFIG_ENCODER_PIN_B) | \
                                  (1ULL << CONFIG_ENCODER_PIN_BTN))

// ==================== NVS命名空间和键名 ====================
#define CONFIG_NVS_NAMESPACE           "lamp_ctrl"
#define CONFIG_NVS_KEY_PEER_MAC        "peer_mac"
#define CONFIG_NVS_KEY_PEER_CHANNEL    "peer_ch"
#define CONFIG_NVS_KEY_IS_PAIRED       "is_paired"
#define CONFIG_NVS_KEY_LAMP_STATE      "lamp_state"

// ==================== 调试配置 ====================
#define CONFIG_SERIAL_BAUD_RATE        115200
#define CONFIG_LOG_LEVEL_DEBUG         1     // 启用调试日志（0=关闭，1=启用）
#define CONFIG_CLEAR_NVS_ON_BOOT       1     // 启动时清除NVS（首次配对时设为1，正常使用设为0）

// ==================== 日志宏定义 ====================
#if CONFIG_LOG_LEVEL_DEBUG
    #define LOG_E(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
    #define LOG_W(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
    #define LOG_I(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define LOG_D(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define LOG_E(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
    #define LOG_W(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
    #define LOG_I(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define LOG_D(fmt, ...) ((void)0)
#endif
