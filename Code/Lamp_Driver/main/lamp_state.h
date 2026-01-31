/**
 * @file lamp_state.h
 * @brief 智能台灯状态定义（控制器和驱动器共享）
 *
 * 本文件定义了台灯系统的所有状态、命令和通信消息格式
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#define NULL  ((void*)0)

// ==================== 设备标识 ====================
#define DEVICE_ID_CONTROLLER  1   // 控制器设备ID
#define DEVICE_ID_DRIVER      10  // 驱动器设备ID

// ==================== 消息类型 ====================
typedef enum {
    MSG_TYPE_PAIRING = 0,  // 配对消息
    MSG_TYPE_DATA = 1      // 数据消息
} message_type_t;

// ==================== 命令类型 ====================
typedef enum {
    CMD_SET_LAMP_STATE = 0,   // 设置灯光状态（亮度+色温）
    CMD_POWER_ON = 1,         // 开灯（恢复上次状态）
    CMD_POWER_OFF = 2,        // 关灯
    CMD_STATE_RESPONSE = 3    // 状态响应（驱动器->控制器）
} command_type_t;

// ==================== 配对状态 ====================
typedef enum {
    PAIRING_STATE_INIT = 0,      // 初始化
    PAIRING_STATE_CHECKING = 1,  // 检查NVS记录
    PAIRING_STATE_SCANNING = 2,  // 扫描信道
    PAIRING_STATE_REQUESTING = 3,// 发送配对请求
    PAIRING_STATE_WAITING = 4,   // 等待响应
    PAIRING_STATE_PAIRED = 5,    // 已配对
    PAIRING_STATE_TIMEOUT = 6    // 超时
} pairing_state_t;

// ==================== 灯光状态 ====================
/**
 * @brief 灯光状态结构体
 *
 * 包含灯光的完整状态信息，用于控制器和驱动器之间同步状态
 */
typedef struct {
    bool is_on;              // 开关状态
    uint16_t brightness;     // 总亮度 (0-511, 9位分辨率)
    float temperature;       // 色温比例 (0.0-1.0, 0=2700K暖白, 1=5000K冷白)
    uint16_t duty_ch0;       // 冷白通道PWM duty (计算值)
    uint16_t duty_ch1;       // 暖白通道PWM duty (计算值)
} __attribute__((packed)) lamp_state_t;

// ==================== 配对消息 ====================
/**
 * @brief 配对消息结构体
 *
 * 用于控制器和驱动器之间的配对握手
 */
typedef struct {
    uint8_t msg_type;        // MSG_TYPE_PAIRING
    uint8_t device_id;       // 设备ID (1=控制器, 10=驱动器)
    uint8_t mac_addr[6];     // 设备MAC地址
    uint8_t channel;         // WiFi信道 (1-13)
    uint32_t timestamp;      // 时间戳（防重放，使用millis()）
} __attribute__((packed)) pairing_message_t;

// ==================== 数据消息 ====================
/**
 * @brief 数据消息结构体
 *
 * 用于控制器向驱动器发送控制命令，或驱动器向控制器返回状态
 */
typedef struct {
    uint8_t msg_type;        // MSG_TYPE_DATA
    uint8_t device_id;       // 发送方设备ID
    uint8_t sender_mac[6];   // 发送方真实MAC地址（用于验证，因为ESP-NOW源MAC可能是临时的）
    uint8_t command;         // 命令类型 (command_type_t)
    uint8_t seq_num;         // 序列号（检测丢包）
    lamp_state_t lamp_state; // 灯光状态
} __attribute__((packed)) data_message_t;

// ==================== 辅助函数声明 ====================

/**
 * @brief 根据亮度和色温计算两个通道的PWM duty值
 *
 * @param state 灯光状态指针
 */
inline void lamp_calculate_duty(lamp_state_t *state) {
    if (state == NULL || !state->is_on) {
        if (state) {
            state->duty_ch0 = 0;
            state->duty_ch1 = 0;
        }
        return;
    }

    // 根据色温比例分配总亮度到两个通道
    state->duty_ch0 = (uint16_t)(state->brightness * state->temperature);        // 冷白 5000K
    state->duty_ch1 = (uint16_t)(state->brightness * (1.0f - state->temperature)); // 暖白 2700K
}

/**
 * @brief 初始化灯光状态为默认值
 *
 * @param state 灯光状态指针
 */
inline void lamp_state_init_default(lamp_state_t *state) {
    if (state == NULL) return;

    state->is_on = false;
    state->brightness = 255;     // 默认中等亮度（50%）
    state->temperature = 0.5f;   // 默认中性色温（3850K）
    state->duty_ch0 = 0;
    state->duty_ch1 = 0;
}
