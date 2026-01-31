/**
 * @file esp_now_ctrl.h
 * @brief 控制器ESP-NOW通信模块
 *
 * 负责ESP-NOW初始化、配对管理、数据发送和接收处理
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <esp_now.h>
#include "lamp_state.h"

// ==================== ESP-NOW上下文结构体 ====================
typedef struct {
    bool is_paired;              // 是否已配对
    uint8_t peer_mac[6];         // 配对的驱动器MAC地址
    uint8_t peer_channel;        // 配对的驱动器WiFi信道
    pairing_state_t state;       // 当前配对状态
    uint8_t scan_channel;        // 当前扫描信道
    uint8_t retry_count;         // 重试计数器
    uint32_t last_request_time;  // 上次发送配对请求时间
    uint8_t seq_num;             // 发送序列号
} espnow_context_t;

// ==================== 公开函数 ====================

/**
 * @brief 初始化ESP-NOW控制器
 *
 * 初始化WiFi（STA模式）和ESP-NOW，注册回调函数
 *
 * @return true 初始化成功
 * @return false 初始化失败
 */
bool espnow_ctrl_init(void);

/**
 * @brief 启动自动配对流程
 *
 * 首先检查NVS是否有配对记录，如有则尝试连接；
 * 否则开始扫描信道广播配对请求
 *
 * @return true 配对成功或已配对
 * @return false 配对失败
 */
bool espnow_ctrl_auto_pair(void);

/**
 * @brief 发送灯光控制命令
 *
 * @param cmd 命令类型
 * @param state 灯光状态（可为NULL，部分命令不需要）
 * @return true 发送成功
 * @return false 发送失败（未配对或发送错误）
 */
bool espnow_ctrl_send_command(command_type_t cmd, const lamp_state_t *state);

/**
 * @brief 快速重连（用于深度睡眠唤醒）
 *
 * 从RTC内存恢复配对信息，跳过扫描直接连接
 *
 * @param peer_mac 驱动器MAC地址（从RTC内存恢复）
 * @param peer_channel 驱动器信道（从RTC内存恢复）
 * @return true 重连成功
 * @return false 重连失败
 */
bool espnow_ctrl_quick_reconnect(const uint8_t *peer_mac, uint8_t peer_channel);

/**
 * @brief 获取当前配对状态
 *
 * @return pairing_state_t 配对状态
 */
pairing_state_t espnow_ctrl_get_pairing_state(void);

/**
 * @brief 检查是否已配对
 *
 * @return true 已配对
 * @return false 未配对
 */
bool espnow_ctrl_is_paired(void);

/**
 * @brief 获取配对的驱动器MAC地址
 *
 * @param mac_out 输出MAC地址缓冲区（至少6字节）
 * @return true 成功获取
 * @return false 未配对或参数错误
 */
bool espnow_ctrl_get_peer_mac(uint8_t *mac_out);

/**
 * @brief 获取配对的驱动器信道
 *
 * @return uint8_t 信道号（1-13），未配对返回0
 */
uint8_t espnow_ctrl_get_peer_channel(void);

/**
 * @brief 配对状态机处理（需在loop中周期调用）
 *
 * 处理配对超时、重试等逻辑
 */
void espnow_ctrl_process(void);

// ==================== 内部回调函数（由esp_now调用） ====================
// 这些函数不应直接调用

/**
 * @brief ESP-NOW发送回调
 */
void espnow_ctrl_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);

/**
 * @brief ESP-NOW接收回调
 */
void espnow_ctrl_on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len);
