/**
 * @file main.cpp
 * @brief 智能台灯控制器主程序
 *
 * 系统架构：
 * - ESP-NOW通信：与驱动器通信
 * - 编码器处理：用户交互输入
 * - 电源管理：深度睡眠和快速唤醒
 */

#include "Arduino.h"
#include "config.h"
#include "lamp_state.h"
#include "esp_now_ctrl.h"
#include "encoder_handler.h"
#include "power_mgmt.h"

// ==================== 全局状态 ====================
static lamp_state_t g_current_lamp_state = {0};
static lamp_state_t g_new_lamp_state = {0};
static bool g_state_changed = false;

// ==================== 启动初始化 ====================

/**
 * @brief 初始化所有模块
 */
bool initialize_system(void) {
    // 初始化电源管理（检测唤醒原因）
    if (!power_mgmt_init()) {
        LOG_E("Power management init failed");
        return false;
    }

    // 打印启动信息
    power_mgmt_print_startup_info();

    // 初始化编码器
    if (!encoder_init()) {
        LOG_E("Encoder init failed");
        return false;
    }

    // 初始化ESP-NOW
    if (!espnow_ctrl_init()) {
        LOG_E("ESP-NOW init failed");
        return false;
    }

    return true;
}

/**
 * @brief 恢复上次状态和配对信息
 */
void restore_state(void) {
    wakeup_reason_t wakeup_reason = power_mgmt_get_wakeup_reason();

    // 尝试从RTC恢复配对信息（快速路径）
    uint8_t peer_mac[6];
    uint8_t peer_channel;

    if (wakeup_reason != WAKEUP_REASON_POWER_ON &&
        power_mgmt_restore_pairing_from_rtc(peer_mac, &peer_channel)) {
        // 深度睡眠唤醒，使用RTC快速重连
        LOG_I("Quick reconnect from RTC...");
        if (espnow_ctrl_quick_reconnect(peer_mac, peer_channel)) {
            LOG_I("Quick reconnect successful");
        } else {
            LOG_W("Quick reconnect failed, will try auto pair");
        }
    }

    // 恢复灯光状态
    if (!power_mgmt_restore_lamp_state_from_rtc(&g_current_lamp_state)) {
        // RTC中没有有效状态，尝试从NVS恢复
        if (!power_mgmt_load_lamp_state(&g_current_lamp_state)) {
            // NVS中也没有，使用默认值
            LOG_I("Using default lamp state");
            lamp_state_init_default(&g_current_lamp_state);
        }
    }

    LOG_I("Lamp state: on=%d, brightness=%u, temp=%.2f",
          g_current_lamp_state.is_on,
          g_current_lamp_state.brightness,
          g_current_lamp_state.temperature);
}

/**
 * @brief 启动配对流程
 */
void start_pairing(void) {
    // 如果还未配对，启动自动配对
    if (!espnow_ctrl_is_paired()) {
        LOG_I("Not paired, starting auto pairing...");
        espnow_ctrl_auto_pair();
    } else {
        LOG_I("Already paired");
    }
}

// ==================== Arduino Setup ====================

void setup() {
    // 初始化串口
    Serial.begin(CONFIG_SERIAL_BAUD_RATE);
    delay(500);  // 增加延迟确保串口稳定

    LOG_I("======================================");
    LOG_I("  Smart Lamp Controller Starting");
    LOG_I("======================================");

    // 调试选项：清除NVS强制重新配对
#if CONFIG_CLEAR_NVS_ON_BOOT
    LOG_W("!!! CONFIG_CLEAR_NVS_ON_BOOT is enabled !!!");
    LOG_W("Clearing NVS for fresh pairing...");
    power_mgmt_clear_nvs();
    LOG_I("NVS cleared. Please set CONFIG_CLEAR_NVS_ON_BOOT to 0 after first successful pairing.");
#endif

    // 初始化所有模块
    if (!initialize_system()) {
        LOG_E("System initialization failed!");
        // 进入深度睡眠避免空转
        delay(1000);
        esp_deep_sleep_start();
        return;
    }

    // 恢复上次状态
    restore_state();

    // 启动配对
    start_pairing();

    // 等待配对完成（最多10秒）
    LOG_I("Waiting for pairing to complete...");
    uint32_t pairing_start = millis();
    while (!espnow_ctrl_is_paired() && (millis() - pairing_start < 10000)) {
        espnow_ctrl_process();
        delay(100);
    }

    if (espnow_ctrl_is_paired()) {
        LOG_I("Pairing completed successfully!");
    } else {
        LOG_W("Pairing timeout, will retry in loop");
    }

    // 配置状态指示LED（可选）
    pinMode(CONFIG_LED_INDICATOR_PIN, OUTPUT);
    digitalWrite(CONFIG_LED_INDICATOR_PIN, LOW);

    LOG_I("System ready, paired: %d", espnow_ctrl_is_paired());
    LOG_I("======================================");
}

// ==================== Arduino Loop ====================

void loop() {
    static uint32_t last_sleep_check = 0;
    uint32_t current_time = millis();

    // 处理ESP-NOW配对状态机
    if (!espnow_ctrl_is_paired()) {
        espnow_ctrl_process();
    }

    // 处理编码器和按键事件
    encoder_event_t event = encoder_process(&g_current_lamp_state, &g_new_lamp_state);

    if (event != ENCODER_EVENT_NONE) {
        // 检查状态是否变化
        if (memcmp(&g_current_lamp_state, &g_new_lamp_state, sizeof(lamp_state_t)) != 0) {
            g_state_changed = true;

            LOG_I("State changed: on=%d, brightness=%u, temp=%.2f",
                  g_new_lamp_state.is_on,
                  g_new_lamp_state.brightness,
                  g_new_lamp_state.temperature);

            // 更新电源管理活动时间
            power_mgmt_update_activity();

            // 保存到RTC（为下次睡眠准备）
            power_mgmt_save_lamp_state_to_rtc(&g_new_lamp_state);
        }
    }

    // 发送状态变化到驱动器
    if (g_state_changed && espnow_ctrl_is_paired()) {
        command_type_t cmd;

        if (!g_current_lamp_state.is_on && g_new_lamp_state.is_on) {
            // 关 -> 开
            cmd = CMD_POWER_ON;
        } else if (g_current_lamp_state.is_on && !g_new_lamp_state.is_on) {
            // 开 -> 关
            cmd = CMD_POWER_OFF;
        } else {
            // 调节亮度或色温
            cmd = CMD_SET_LAMP_STATE;
        }

        if (espnow_ctrl_send_command(cmd, &g_new_lamp_state)) {
            LOG_D("Command sent successfully");
            // 更新当前状态
            memcpy(&g_current_lamp_state, &g_new_lamp_state, sizeof(lamp_state_t));
        } else {
            LOG_W("Failed to send command");
        }

        g_state_changed = false;
    }

    // 状态指示LED闪烁（已配对时常亮，未配对时闪烁）
    if (espnow_ctrl_is_paired()) {
        digitalWrite(CONFIG_LED_INDICATOR_PIN, HIGH);
    } else {
        // 每500ms闪烁
        digitalWrite(CONFIG_LED_INDICATOR_PIN, (millis() / 500) % 2);
    }

    // 定期检查是否应该进入睡眠（每500ms检查一次）
    if (current_time - last_sleep_check >= CONFIG_SLEEP_CHECK_INTERVAL_MS) {
        last_sleep_check = current_time;

        uint32_t last_activity = encoder_get_last_activity_time();

        if (power_mgmt_should_sleep(last_activity) && espnow_ctrl_is_paired()) {
            LOG_I("Idle timeout, entering deep sleep...");

            // 保存状态到NVS（每次睡眠都保存，确保数据持久化）
            power_mgmt_enter_deep_sleep(true);
            // 此函数不会返回
        }
    }

    // 短延迟避免忙等待
    delay(10);
}
