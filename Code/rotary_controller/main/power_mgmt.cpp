/**
 * @file power_mgmt.cpp
 * @brief 电源管理模块实现
 */

#include "power_mgmt.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <driver/rtc_io.h>
#include <string.h>

// ==================== RTC内存变量定义 ====================
RTC_DATA_ATTR bool rtc_is_paired = false;
RTC_DATA_ATTR uint8_t rtc_peer_mac[6] = {0};
RTC_DATA_ATTR uint8_t rtc_peer_channel = 0;
RTC_DATA_ATTR lamp_state_t rtc_lamp_state = {0};

// ==================== 模块私有变量 ====================
static Preferences g_preferences;
static uint32_t g_last_activity_time = 0;
static wakeup_reason_t g_wakeup_reason = WAKEUP_REASON_UNKNOWN;

// ==================== 辅助函数 ====================

/**
 * @brief 检查MAC地址是否全为0
 */
static bool is_mac_zero(const uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 打印MAC地址
 */
static void print_mac(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ==================== 公开函数实现 ====================

/**
 * @brief 初始化电源管理
 */
bool power_mgmt_init(void) {
    LOG_I("Initializing power management...");

    // 检测唤醒原因
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    switch (wakeup_cause) {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            g_wakeup_reason = WAKEUP_REASON_POWER_ON;
            LOG_I("Power on reset or software reset");
            break;

        case ESP_SLEEP_WAKEUP_GPIO:
        {
            uint64_t wakeup_pin_mask = esp_sleep_get_gpio_wakeup_status();
            LOG_I("Wakeup by GPIO, mask: 0x%llx", wakeup_pin_mask);

            if (wakeup_pin_mask & (1ULL << CONFIG_ENCODER_PIN_A) ||
                wakeup_pin_mask & (1ULL << CONFIG_ENCODER_PIN_B)) {
                g_wakeup_reason = WAKEUP_REASON_ENCODER;
            } else if (wakeup_pin_mask & (1ULL << CONFIG_ENCODER_PIN_BTN)) {
                g_wakeup_reason = WAKEUP_REASON_BUTTON;
            } else {
                g_wakeup_reason = WAKEUP_REASON_TIMEOUT;
            }
            break;
        }

        default:
            g_wakeup_reason = WAKEUP_REASON_TIMEOUT;
            LOG_I("Wakeup by other reason: %d", wakeup_cause);
            break;
    }

    // 配置GPIO唤醒（下降沿触发）
    esp_deep_sleep_enable_gpio_wakeup(CONFIG_WAKEUP_GPIO_MASK, ESP_GPIO_WAKEUP_GPIO_LOW);

    // 配置GPIO下拉（编码器引脚）
    gpio_pullup_dis((gpio_num_t)CONFIG_ENCODER_PIN_A);
    gpio_pulldown_en((gpio_num_t)CONFIG_ENCODER_PIN_A);
    gpio_pullup_dis((gpio_num_t)CONFIG_ENCODER_PIN_B);
    gpio_pulldown_en((gpio_num_t)CONFIG_ENCODER_PIN_B);

    // 按键保持上拉
    gpio_pullup_en((gpio_num_t)CONFIG_ENCODER_PIN_BTN);
    gpio_pulldown_dis((gpio_num_t)CONFIG_ENCODER_PIN_BTN);

    g_last_activity_time = millis();

    LOG_I("Power management initialized");
    return true;
}

/**
 * @brief 获取唤醒原因
 */
wakeup_reason_t power_mgmt_get_wakeup_reason(void) {
    return g_wakeup_reason;
}

/**
 * @brief 进入深度睡眠
 */
void power_mgmt_enter_deep_sleep(bool save_to_nvs) {
    LOG_I("Entering deep sleep...");

    // 保存状态到RTC（总是保存）
    // RTC变量已经在其他地方更新，这里仅打印确认
    LOG_I("RTC paired: %d", rtc_is_paired);
    if (rtc_is_paired) {
        LOG_I("RTC peer MAC: ");
        print_mac(rtc_peer_mac);
        Serial.printf(", channel: %d\n", rtc_peer_channel);
    }

    // 可选：保存到NVS（持久化存储）
    if (save_to_nvs) {
        if (rtc_is_paired) {
            power_mgmt_save_pairing_info(rtc_peer_mac, rtc_peer_channel);
        }
        power_mgmt_save_lamp_state(&rtc_lamp_state);
        LOG_I("State saved to NVS");
    }

    // 确保串口输出完成
    Serial.flush();
    delay(100);

    // 进入深度睡眠
    esp_deep_sleep_start();
}

/**
 * @brief 检查是否应该睡眠
 */
bool power_mgmt_should_sleep(uint32_t last_activity_time) {
    uint32_t idle_time = millis() - last_activity_time;
    return (idle_time >= CONFIG_SLEEP_TIMEOUT_MS);
}

/**
 * @brief 更新活动时间
 */
void power_mgmt_update_activity(void) {
    g_last_activity_time = millis();
}

/**
 * @brief 获取最后活动时间
 */
uint32_t power_mgmt_get_last_activity_time(void) {
    return g_last_activity_time;
}

// ==================== NVS操作 ====================

/**
 * @brief 保存配对信息到NVS
 */
bool power_mgmt_save_pairing_info(const uint8_t *peer_mac, uint8_t peer_channel) {
    if (peer_mac == NULL) {
        return false;
    }

    if (!g_preferences.begin(CONFIG_NVS_NAMESPACE, false)) {
        LOG_E("Failed to open NVS namespace");
        return false;
    }

    g_preferences.putBytes(CONFIG_NVS_KEY_PEER_MAC, peer_mac, 6);
    g_preferences.putUChar(CONFIG_NVS_KEY_PEER_CHANNEL, peer_channel);
    g_preferences.putBool(CONFIG_NVS_KEY_IS_PAIRED, true);
    g_preferences.end();

    LOG_I("Pairing info saved to NVS");
    return true;
}

/**
 * @brief 从NVS读取配对信息
 */
bool power_mgmt_load_pairing_info(uint8_t *peer_mac_out, uint8_t *peer_channel_out) {
    if (peer_mac_out == NULL || peer_channel_out == NULL) {
        return false;
    }

    if (!g_preferences.begin(CONFIG_NVS_NAMESPACE, true)) {
        LOG_W("NVS namespace not found (first boot?)");
        return false;
    }

    bool is_paired = g_preferences.getBool(CONFIG_NVS_KEY_IS_PAIRED, false);
    if (!is_paired) {
        g_preferences.end();
        LOG_W("No pairing info in NVS");
        return false;
    }

    size_t len = g_preferences.getBytes(CONFIG_NVS_KEY_PEER_MAC, peer_mac_out, 6);
    if (len != 6) {
        g_preferences.end();
        LOG_E("Invalid MAC address in NVS");
        return false;
    }

    *peer_channel_out = g_preferences.getUChar(CONFIG_NVS_KEY_PEER_CHANNEL, 1);
    g_preferences.end();

    // 检查MAC是否有效
    if (is_mac_zero(peer_mac_out)) {
        LOG_W("Zero MAC in NVS");
        return false;
    }

    LOG_I("Pairing info loaded from NVS: ");
    print_mac(peer_mac_out);
    Serial.printf(", channel: %d\n", *peer_channel_out);

    return true;
}

/**
 * @brief 保存灯光状态到NVS
 */
bool power_mgmt_save_lamp_state(const lamp_state_t *state) {
    if (state == NULL) {
        return false;
    }

    if (!g_preferences.begin(CONFIG_NVS_NAMESPACE, false)) {
        LOG_E("Failed to open NVS namespace");
        return false;
    }

    g_preferences.putBytes(CONFIG_NVS_KEY_LAMP_STATE, state, sizeof(lamp_state_t));
    g_preferences.end();

    LOG_D("Lamp state saved to NVS");
    return true;
}

/**
 * @brief 从NVS读取灯光状态
 */
bool power_mgmt_load_lamp_state(lamp_state_t *state_out) {
    if (state_out == NULL) {
        return false;
    }

    if (!g_preferences.begin(CONFIG_NVS_NAMESPACE, true)) {
        return false;
    }

    size_t len = g_preferences.getBytes(CONFIG_NVS_KEY_LAMP_STATE, state_out, sizeof(lamp_state_t));
    g_preferences.end();

    if (len != sizeof(lamp_state_t)) {
        LOG_W("Invalid lamp state in NVS");
        return false;
    }

    LOG_D("Lamp state loaded from NVS: brightness=%u, temp=%.2f",
          state_out->brightness, state_out->temperature);
    return true;
}

/**
 * @brief 清除NVS
 */
bool power_mgmt_clear_nvs(void) {
    if (!g_preferences.begin(CONFIG_NVS_NAMESPACE, false)) {
        LOG_E("Failed to open NVS namespace");
        return false;
    }

    g_preferences.clear();
    g_preferences.end();

    LOG_W("NVS cleared");
    return true;
}

// ==================== RTC内存操作 ====================

/**
 * @brief 从RTC恢复配对信息
 */
bool power_mgmt_restore_pairing_from_rtc(uint8_t *peer_mac_out, uint8_t *peer_channel_out) {
    if (peer_mac_out == NULL || peer_channel_out == NULL) {
        return false;
    }

    if (!rtc_is_paired || is_mac_zero(rtc_peer_mac)) {
        LOG_W("No valid pairing info in RTC");
        return false;
    }

    memcpy(peer_mac_out, rtc_peer_mac, 6);
    *peer_channel_out = rtc_peer_channel;

    LOG_I("Pairing info restored from RTC: ");
    print_mac(peer_mac_out);
    Serial.printf(", channel: %d\n", *peer_channel_out);

    return true;
}

/**
 * @brief 保存配对信息到RTC
 */
void power_mgmt_save_pairing_to_rtc(const uint8_t *peer_mac, uint8_t peer_channel) {
    if (peer_mac == NULL) {
        return;
    }

    memcpy(rtc_peer_mac, peer_mac, 6);
    rtc_peer_channel = peer_channel;
    rtc_is_paired = true;

    LOG_D("Pairing info saved to RTC");
}

/**
 * @brief 从RTC恢复灯光状态
 */
bool power_mgmt_restore_lamp_state_from_rtc(lamp_state_t *state_out) {
    if (state_out == NULL) {
        return false;
    }

    // 检查RTC状态是否有效（简单检查brightness范围）
    if (rtc_lamp_state.brightness > CONFIG_MAX_BRIGHTNESS) {
        LOG_W("Invalid lamp state in RTC");
        return false;
    }

    memcpy(state_out, &rtc_lamp_state, sizeof(lamp_state_t));

    LOG_D("Lamp state restored from RTC: brightness=%u, temp=%.2f",
          state_out->brightness, state_out->temperature);
    return true;
}

/**
 * @brief 保存灯光状态到RTC
 */
void power_mgmt_save_lamp_state_to_rtc(const lamp_state_t *state) {
    if (state == NULL) {
        return;
    }

    memcpy(&rtc_lamp_state, state, sizeof(lamp_state_t));
    LOG_D("Lamp state saved to RTC");
}

/**
 * @brief 打印启动信息
 */
void power_mgmt_print_startup_info(void) {
    LOG_I("=== Smart Lamp Controller v1.0 ===");
    LOG_I("Compiled: %s %s", __DATE__, __TIME__);
    LOG_I("Free heap: %lu bytes", (unsigned long)ESP.getFreeHeap());

    switch (g_wakeup_reason) {
        case WAKEUP_REASON_POWER_ON:
            LOG_I("Wakeup: Power On");
            break;
        case WAKEUP_REASON_ENCODER:
            LOG_I("Wakeup: Encoder Rotation");
            break;
        case WAKEUP_REASON_BUTTON:
            LOG_I("Wakeup: Button Press");
            break;
        case WAKEUP_REASON_TIMEOUT:
            LOG_I("Wakeup: Timeout/Other");
            break;
        default:
            LOG_I("Wakeup: Unknown");
            break;
    }
}
