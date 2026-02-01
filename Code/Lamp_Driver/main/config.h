/**
 * @file config.h
 * @brief 驱动器配置定义
 *
 * 集中管理所有配置参数，便于维护和调整
 */

#pragma once

#include <stdint.h>

// ==================== GPIO定义 ====================
#define CONFIG_LED_CH0_PIN             0   // 冷白LED (5000K)
#define CONFIG_LED_CH1_PIN             1   // 暖白LED (2700K)

// ==================== PWM参数 ====================
#define CONFIG_PWM_RESOLUTION          9     // PWM分辨率（9位=0-511）
#define CONFIG_PWM_FREQUENCY           20000 // PWM频率 20KHz
#define CONFIG_MAX_DUTY                511   // 最大duty值 (2^9 - 1)
#define CONFIG_MIN_DUTY                0     // 最小duty值

// ==================== LED淡入淡出参数 ====================
#define CONFIG_LED_STEP_FADE_TIME_MS   10     // 每步淡入淡出时间
#define CONFIG_LED_POWER_FADE_TIME_MS  2000  // 开关灯淡入淡出时间

// ==================== 色温范围 ====================
#define CONFIG_COLOR_TEMP_MAX          5000  // 最大色温 (K)
#define CONFIG_COLOR_TEMP_MIN          2700  // 最小色温 (K)

// ==================== ESP-NOW配置 ====================
#define CONFIG_ESPNOW_WIFI_CHANNEL     1     // 默认WiFi信道
#define CONFIG_ESPNOW_MAX_CHANNEL      13    // 最大信道

// ==================== NVS命名空间和键名 ====================
#define CONFIG_NVS_NAMESPACE           "lamp_drv"
#define CONFIG_NVS_KEY_LAMP_STATE      "lamp_state"
#define CONFIG_NVS_KEY_LAST_BRIGHTNESS "last_bright"
#define CONFIG_NVS_KEY_CONTROLLERS     "controllers"  // 控制器MAC列表

// ==================== 调试配置 ====================
#define CONFIG_SERIAL_BAUD_RATE        115200
#define CONFIG_LOG_LEVEL_DEBUG         0     // 启用调试日志（0=关闭，1=启用）

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
