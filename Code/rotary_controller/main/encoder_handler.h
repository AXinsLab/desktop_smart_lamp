/**
 * @file encoder_handler.h
 * @brief 编码器和按键处理模块
 *
 * 负责编码器旋转、按键事件的检测和处理，根据灯光状态执行相应操作
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lamp_state.h"

// ==================== 编码器模式 ====================
typedef enum {
    ENCODER_MODE_BRIGHTNESS = 0,  // 亮度调节模式（默认）
    ENCODER_MODE_TEMPERATURE = 1  // 色温调节模式（按住按键时）
} encoder_mode_t;

// ==================== 编码器事件 ====================
typedef enum {
    ENCODER_EVENT_NONE = 0,
    ENCODER_EVENT_ROTATE_CW,      // 顺时针旋转
    ENCODER_EVENT_ROTATE_CCW,     // 逆时针旋转
    ENCODER_EVENT_BTN_PRESS,      // 按键按下
    ENCODER_EVENT_BTN_RELEASE,    // 按键释放
    ENCODER_EVENT_BTN_CLICK,      // 短按
    ENCODER_EVENT_BTN_LONG_PRESS, // 长按
    ENCODER_EVENT_BTN_RESET       // 超长按（5秒，重置配对）
} encoder_event_t;

// ==================== 公开函数 ====================

/**
 * @brief 初始化编码器和按键
 *
 * 配置编码器中断、按键防抖等
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool encoder_init(void);

/**
 * @brief 处理编码器和按键事件（需在loop中周期调用）
 *
 * 检测编码器旋转、按键状态，更新灯光状态并触发ESP-NOW发送
 *
 * @param current_state 当前灯光状态指针（输入）
 * @param new_state 新灯光状态指针（输出，发生变化时更新）
 * @return encoder_event_t 检测到的事件类型
 */
encoder_event_t encoder_process(const lamp_state_t *current_state, lamp_state_t *new_state);

/**
 * @brief 获取当前编码器模式
 *
 * @return encoder_mode_t 当前模式
 */
encoder_mode_t encoder_get_mode(void);

/**
 * @brief 重置编码器值
 *
 * 用于深度睡眠唤醒后重置编码器内部计数
 */
void encoder_reset(void);

/**
 * @brief 检查编码器是否有活动（用于电源管理）
 *
 * @return true 最近有活动
 * @return false 无活动
 */
bool encoder_has_activity(void);

/**
 * @brief 获取上次活动时间（毫秒）
 *
 * @return uint32_t 上次活动的时间戳（millis()）
 */
uint32_t encoder_get_last_activity_time(void);
