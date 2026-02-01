/**
 * @file power_mgmt.h
 * @brief 电源管理模块
 *
 * 负责深度睡眠、唤醒处理、NVS存储
 * 注：ESP32-C2无RTC内存，完全依赖NVS存储
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_sleep.h>
#include "lamp_state.h"

// ==================== 唤醒原因 ====================
typedef enum {
    WAKEUP_REASON_UNKNOWN = 0,
    WAKEUP_REASON_POWER_ON,      // 首次上电
    WAKEUP_REASON_ENCODER,       // 编码器唤醒
    WAKEUP_REASON_BUTTON,        // 按键唤醒
    WAKEUP_REASON_TIMEOUT        // 其他原因
} wakeup_reason_t;

// ==================== 公开函数 ====================

/**
 * @brief 初始化电源管理模块
 *
 * 配置GPIO唤醒源、检查唤醒原因、恢复RTC状态
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool power_mgmt_init(void);

/**
 * @brief 获取唤醒原因
 *
 * @return wakeup_reason_t 唤醒原因
 */
wakeup_reason_t power_mgmt_get_wakeup_reason(void);

/**
 * @brief 进入深度睡眠
 *
 * 保存状态到NVS，然后进入深度睡眠
 */
void power_mgmt_enter_deep_sleep(void);

/**
 * @brief 检查是否应该进入睡眠
 *
 * 根据配置的超时时间和最后活动时间判断
 *
 * @param last_activity_time 最后活动时间（millis()）
 * @return true 应该睡眠
 * @return false 不应该睡眠
 */
bool power_mgmt_should_sleep(uint32_t last_activity_time);

/**
 * @brief 更新活动时间（防止睡眠）
 *
 * 编码器或按键活动时调用此函数重置睡眠计时器
 */
void power_mgmt_update_activity(void);

/**
 * @brief 获取最后活动时间
 *
 * @return uint32_t 最后活动时间戳（millis()）
 */
uint32_t power_mgmt_get_last_activity_time(void);

// ==================== NVS操作封装 ====================

/**
 * @brief 保存配对信息到NVS
 *
 * @param peer_mac 驱动器MAC地址
 * @param peer_channel 驱动器信道
 * @return true 保存成功
 * @return false 保存失败
 */
bool power_mgmt_save_pairing_info(const uint8_t *peer_mac, uint8_t peer_channel);

/**
 * @brief 从NVS读取配对信息
 *
 * @param peer_mac_out 输出MAC地址缓冲区（至少6字节）
 * @param peer_channel_out 输出信道指针
 * @return true 读取成功
 * @return false 读取失败（可能是首次启动）
 */
bool power_mgmt_load_pairing_info(uint8_t *peer_mac_out, uint8_t *peer_channel_out);

/**
 * @brief 保存灯光状态到NVS
 *
 * @param state 灯光状态指针
 * @return true 保存成功
 * @return false 保存失败
 */
bool power_mgmt_save_lamp_state(const lamp_state_t *state);

/**
 * @brief 从NVS读取灯光状态
 *
 * @param state_out 输出灯光状态指针
 * @return true 读取成功
 * @return false 读取失败
 */
bool power_mgmt_load_lamp_state(lamp_state_t *state_out);

/**
 * @brief 清除NVS中的所有数据（调试用）
 *
 * @return true 清除成功
 * @return false 清除失败
 */
bool power_mgmt_clear_nvs(void);

/**
 * @brief 打印启动信息和唤醒原因（调试用）
 */
void power_mgmt_print_startup_info(void);
