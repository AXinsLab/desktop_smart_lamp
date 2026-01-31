/**
 * @file main.cpp
 * @brief 智能台灯驱动器主程序
 *
 * 系统架构：
 * - ESP-NOW通信：接收控制器命令
 * - LED控制：执行淡入淡出和状态管理
 */

#include "Arduino.h"
#include "config.h"
#include "lamp_state.h"
#include "esp_now_driver.h"
#include "led_controller.h"
#include <WiFi.h>
#include <Preferences.h>

// ==================== 启动初始化 ====================

/**
 * @brief 初始化所有模块
 */
bool initialize_system(void) {
    // 初始化LED控制器
    if (!led_ctrl_init()) {
        LOG_E("LED controller init failed");
        return false;
    }

    // 初始化ESP-NOW
    if (!espnow_driver_init()) {
        LOG_E("ESP-NOW driver init failed");
        return false;
    }

    return true;
}

// ==================== Arduino Setup ====================

void setup() {
    // 初始化串口
    Serial.begin(CONFIG_SERIAL_BAUD_RATE);
    delay(500);  // 增加延迟确保串口稳定

    LOG_I("======================================");
    LOG_I("    Smart Lamp Driver Starting");
    LOG_I("======================================");

    // 调试选项：清除NVS（通常不需要）
#if CONFIG_CLEAR_NVS_ON_BOOT
    LOG_W("!!! CONFIG_CLEAR_NVS_ON_BOOT is enabled !!!");
    LOG_W("Clearing NVS...");
    Preferences prefs;
    prefs.begin(CONFIG_NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    LOG_I("NVS cleared.");
#endif

    // 初始化所有模块
    if (!initialize_system()) {
        LOG_E("System initialization failed!");
        while (1) {
            delay(1000);  // 停止运行
        }
    }

    // 打印驱动器信息
    LOG_I("Driver MAC: %s", WiFi.macAddress().c_str());
    LOG_I("WiFi Channel: %ld", WiFi.channel());
    LOG_I("Free heap: %lu bytes", (unsigned long)ESP.getFreeHeap());

    LOG_I("System ready, waiting for controller...");
    LOG_I("======================================");
}

// ==================== Arduino Loop ====================

void loop() {
    // 驱动器主要由中断驱动（ESP-NOW接收回调）
    // loop中无需做特别处理，仅保持运行

    // 短延迟避免忙等待
    delay(10);
}
