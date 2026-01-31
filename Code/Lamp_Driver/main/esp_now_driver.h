/**
 * @file esp_now_driver.h
 * @brief 驱动器ESP-NOW通信模块
 *
 * 负责ESP-NOW初始化、接收控制命令、发送状态响应、配对管理
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_now.h>
#include "lamp_state.h"

// ==================== 公开函数 ====================

/**
 * @brief 初始化ESP-NOW驱动器
 *
 * 初始化WiFi（STA模式）和ESP-NOW，注册回调函数
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool espnow_driver_init(void);

/**
 * @brief 发送状态响应给控制器
 *
 * @param controller_mac 控制器MAC地址
 * @param state 当前灯光状态
 * @return true 发送成功
 * @return false 发送失败
 */
bool espnow_driver_send_state_response(const uint8_t *controller_mac, const lamp_state_t *state);

/**
 * @brief 发送配对响应给控制器
 *
 * @param controller_mac 控制器MAC地址
 * @param controller_channel 控制器信道
 * @return true 发送成功
 * @return false 发送失败
 */
bool espnow_driver_send_pairing_response(const uint8_t *controller_mac, uint8_t controller_channel);

/**
 * @brief 检查是否已配对指定控制器
 *
 * @param controller_mac 控制器MAC地址
 * @return true 已配对
 * @return false 未配对
 */
bool espnow_driver_is_paired_with(const uint8_t *controller_mac);

/**
 * @brief 添加控制器为peer
 *
 * @param controller_mac 控制器MAC地址
 * @param channel 信道号
 * @return true 添加成功
 * @return false 添加失败
 */
bool espnow_driver_add_controller(const uint8_t *controller_mac, uint8_t channel);

/**
 * @brief 处理接收到的命令（由回调调用）
 *
 * @param cmd 命令类型
 * @param state 灯光状态（可能为NULL）
 * @param controller_mac 控制器MAC地址
 */
void espnow_driver_handle_command(command_type_t cmd, const lamp_state_t *state, const uint8_t *controller_mac);

// ==================== 内部回调函数（由esp_now调用） ====================

/**
 * @brief ESP-NOW发送回调
 */
void espnow_driver_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);

/**
 * @brief ESP-NOW接收回调
 */
void espnow_driver_on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len);
