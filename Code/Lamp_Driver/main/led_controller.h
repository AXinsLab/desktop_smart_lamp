/**
 * @file led_controller.h
 * @brief LED控制模块
 *
 * 负责LEDC初始化、淡入淡出控制、亮度和色温管理
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lamp_state.h"

// ==================== 公开函数 ====================

/**
 * @brief 初始化LED控制器
 *
 * 配置LEDC时钟源、PWM通道等
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool led_ctrl_init(void);

/**
 * @brief 设置灯光状态
 *
 * 根据目标状态控制LED淡入淡出到指定亮度和色温
 *
 * @param target_state 目标灯光状态
 * @return true 设置成功
 * @return false 设置失败
 */
bool led_ctrl_set_state(const lamp_state_t *target_state);

/**
 * @brief 开灯（淡入）
 *
 * 从当前状态淡入到指定状态
 *
 * @param target_state 目标灯光状态（包含亮度和色温）
 * @return true 操作成功
 * @return false 操作失败
 */
bool led_ctrl_power_on(const lamp_state_t *target_state);

/**
 * @brief 关灯（淡出）
 *
 * 从当前状态淡出到0
 *
 * @return true 操作成功
 * @return false 操作失败
 */
bool led_ctrl_power_off(void);

/**
 * @brief 获取当前灯光状态
 *
 * @param state_out 输出当前状态指针
 * @return true 成功获取
 * @return false 获取失败
 */
bool led_ctrl_get_current_state(lamp_state_t *state_out);

/**
 * @brief 保存上次状态（关灯前调用）
 *
 * 保存当前亮度和色温，以便下次开灯时恢复
 */
void led_ctrl_save_last_state(void);

/**
 * @brief 恢复上次状态
 *
 * 恢复上次关灯前的亮度和色温
 *
 * @param state_out 输出上次状态指针
 * @return true 成功恢复
 * @return false 无上次状态或恢复失败
 */
bool led_ctrl_restore_last_state(lamp_state_t *state_out);
