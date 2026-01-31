/**
 * @file esp_now_ctrl.cpp
 * @brief 控制器ESP-NOW通信模块实现
 */

#include "esp_now_ctrl.h"
#include "config.h"
#include "power_mgmt.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

// ==================== 模块私有变量 ====================
static espnow_context_t g_espnow_ctx = {0};
static esp_now_peer_info_t g_peer_info = {0};
static uint8_t g_broadcast_mac[6] = CONFIG_PAIRING_BROADCAST_MAC;

// ==================== 辅助函数 ====================

/**
 * @brief 打印MAC地址（调试用）
 */
static void print_mac(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief 比较两个MAC地址是否相等
 */
static bool mac_equal(const uint8_t *mac1, const uint8_t *mac2) {
    return memcmp(mac1, mac2, 6) == 0;
}

/**
 * @brief 添加peer到ESP-NOW
 */
static bool add_peer(const uint8_t *mac_addr, uint8_t channel) {
    // 删除旧peer（如果存在）
    esp_now_del_peer(mac_addr);

    // 配置新peer
    memset(&g_peer_info, 0, sizeof(esp_now_peer_info_t));
    memcpy(g_peer_info.peer_addr, mac_addr, 6);
    g_peer_info.channel = channel;
    g_peer_info.encrypt = false;

    esp_err_t result = esp_now_add_peer(&g_peer_info);
    if (result != ESP_OK) {
        LOG_E("Failed to add peer, error: 0x%x", result);
        return false;
    }

    LOG_I("Peer added: ");
    print_mac(mac_addr);
    Serial.printf(" on channel %d\n", channel);
    return true;
}

// ==================== ESP-NOW回调函数 ====================

/**
 * @brief 发送回调
 */
void espnow_ctrl_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        LOG_D("Send OK to ");
        print_mac(mac_addr);
        Serial.println();
    } else {
        LOG_W("Send FAIL to ");
        print_mac(mac_addr);
        Serial.println();
    }
}

/**
 * @brief 接收回调
 */
void espnow_ctrl_on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (data == NULL || len < 1) {
        return;
    }

    uint8_t msg_type = data[0];

    // 处理配对响应
    if (msg_type == MSG_TYPE_PAIRING) {
        if (len < sizeof(pairing_message_t)) {
            LOG_W("Invalid pairing message size: %d", len);
            return;
        }

        pairing_message_t *pairing_msg = (pairing_message_t *)data;

        LOG_D("Pairing message received: type=%d, device_id=%d",
              pairing_msg->msg_type, pairing_msg->device_id);

        // 检查是否来自驱动器
        if (pairing_msg->device_id == DEVICE_ID_DRIVER) {
            LOG_I("=== Pairing Response Received ===");

            // 关键修复：ESP-NOW单播响应时，回调的MAC也可能是临时MAC
            // 应该使用消息体中的真实MAC地址
            LOG_I("Driver MAC (from ESP-NOW callback): ");
            print_mac(mac_addr);
            Serial.printf(" (可能是临时MAC)");
            Serial.printf("\nDriver MAC (from message body): ");
            print_mac(pairing_msg->mac_addr);
            Serial.printf(" (真实MAC)");
            Serial.printf("\nDriver channel: %d\n", pairing_msg->channel);

            // 使用消息体中的真实MAC地址保存配对信息
            memcpy(g_espnow_ctx.peer_mac, pairing_msg->mac_addr, 6);
            g_espnow_ctx.peer_channel = pairing_msg->channel;
            g_espnow_ctx.is_paired = true;
            g_espnow_ctx.state = PAIRING_STATE_PAIRED;

            // 添加为peer（使用真实MAC）
            if (add_peer(g_espnow_ctx.peer_mac, g_espnow_ctx.peer_channel)) {
                LOG_I("Driver added as peer successfully");
            } else {
                LOG_E("Failed to add driver as peer");
            }

            // 保存到NVS和RTC
            power_mgmt_save_pairing_info(g_espnow_ctx.peer_mac, g_espnow_ctx.peer_channel);
            power_mgmt_save_pairing_to_rtc(g_espnow_ctx.peer_mac, g_espnow_ctx.peer_channel);

            LOG_I("Pairing completed in %lu ms", millis() - g_espnow_ctx.last_request_time);
            LOG_I("Saved peer MAC (should be real MAC): ");
            print_mac(g_espnow_ctx.peer_mac);
            Serial.printf("\nis_paired = %d\n", g_espnow_ctx.is_paired);
        } else {
            LOG_W("Pairing response from unknown device type: %d", pairing_msg->device_id);
        }
    }
    // 处理数据响应（驱动器返回的状态）
    else if (msg_type == MSG_TYPE_DATA) {
        if (len < sizeof(data_message_t)) {
            LOG_W("Invalid data message size: %d", len);
            return;
        }

        data_message_t *data_msg = (data_message_t *)data;

        if (data_msg->command == CMD_STATE_RESPONSE) {
            LOG_D("State response: brightness=%u, temp=%.2f, on=%d",
                  data_msg->lamp_state.brightness,
                  data_msg->lamp_state.temperature,
                  data_msg->lamp_state.is_on);

            // 更新RTC状态
            power_mgmt_save_lamp_state_to_rtc(&data_msg->lamp_state);
        }
    }
}

// ==================== 公开函数实现 ====================

/**
 * @brief 初始化ESP-NOW控制器
 */
bool espnow_ctrl_init(void) {
    LOG_I("Initializing ESP-NOW controller...");

    // 初始化WiFi（STA模式）
    WiFi.mode(WIFI_STA);
    WiFi.STA.begin();

    // 打印MAC地址
    LOG_I("Controller MAC: %s", WiFi.macAddress().c_str());

    // 初始化ESP-NOW
    if (esp_now_init() != ESP_OK) {
        LOG_E("ESP-NOW init failed");
        return false;
    }

    // 注册回调
    esp_now_register_send_cb(espnow_ctrl_on_data_sent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(espnow_ctrl_on_data_recv));

    // 初始化上下文
    g_espnow_ctx.state = PAIRING_STATE_INIT;
    g_espnow_ctx.is_paired = false;
    g_espnow_ctx.scan_channel = CONFIG_ESPNOW_WIFI_CHANNEL;
    g_espnow_ctx.retry_count = 0;
    g_espnow_ctx.seq_num = 0;

    LOG_I("ESP-NOW initialized successfully");
    return true;
}

/**
 * @brief 快速重连（深度睡眠唤醒）
 */
bool espnow_ctrl_quick_reconnect(const uint8_t *peer_mac, uint8_t peer_channel) {
    if (peer_mac == NULL) {
        return false;
    }

    LOG_I("Quick reconnect to channel %d", peer_channel);

    // 设置WiFi信道
    ESP_ERROR_CHECK(esp_wifi_set_channel(peer_channel, WIFI_SECOND_CHAN_NONE));

    // 保存到上下文
    memcpy(g_espnow_ctx.peer_mac, peer_mac, 6);
    g_espnow_ctx.peer_channel = peer_channel;
    g_espnow_ctx.is_paired = true;
    g_espnow_ctx.state = PAIRING_STATE_PAIRED;

    // 添加peer
    if (!add_peer(peer_mac, peer_channel)) {
        return false;
    }

    LOG_I("Quick reconnect successful");
    return true;
}

/**
 * @brief 启动自动配对
 */
bool espnow_ctrl_auto_pair(void) {
    LOG_I("Starting auto pairing...");

    // 首先尝试从NVS恢复
    uint8_t saved_mac[6];
    uint8_t saved_channel;

    if (power_mgmt_load_pairing_info(saved_mac, &saved_channel)) {
        LOG_I("Found pairing info in NVS, trying to reconnect...");

        // 尝试快速重连
        if (espnow_ctrl_quick_reconnect(saved_mac, saved_channel)) {
            return true;
        }

        LOG_W("Quick reconnect failed, starting scan...");
    }

    // NVS中没有记录或重连失败，开始扫描配对
    g_espnow_ctx.state = PAIRING_STATE_SCANNING;
    g_espnow_ctx.scan_channel = 1;
    g_espnow_ctx.retry_count = 0;

    return false; // 需要在process()中继续处理
}

/**
 * @brief 配对状态机处理
 */
void espnow_ctrl_process(void) {
    if (g_espnow_ctx.state == PAIRING_STATE_PAIRED) {
        return; // 已配对，无需处理
    }

    uint32_t current_time = millis();

    switch (g_espnow_ctx.state) {
        case PAIRING_STATE_SCANNING:
        case PAIRING_STATE_REQUESTING:
        {
            LOG_D("=== Scanning channel %d ===", g_espnow_ctx.scan_channel);

            // 切换到当前扫描信道
            ESP_ERROR_CHECK(esp_wifi_set_channel(g_espnow_ctx.scan_channel, WIFI_SECOND_CHAN_NONE));

            // 添加广播地址为peer
            add_peer(g_broadcast_mac, g_espnow_ctx.scan_channel);

            // 发送配对请求
            pairing_message_t pairing_msg = {0};
            pairing_msg.msg_type = MSG_TYPE_PAIRING;
            pairing_msg.device_id = DEVICE_ID_CONTROLLER;
            WiFi.macAddress(pairing_msg.mac_addr);
            pairing_msg.channel = g_espnow_ctx.scan_channel;
            pairing_msg.timestamp = millis();

            LOG_D("Sending pairing request: controller_MAC=");
            print_mac(pairing_msg.mac_addr);
            Serial.printf(", channel=%d\n", pairing_msg.channel);

            esp_err_t result = esp_now_send(g_broadcast_mac, (uint8_t *)&pairing_msg, sizeof(pairing_msg));
            if (result == ESP_OK) {
                LOG_I("Pairing request sent on channel %d", g_espnow_ctx.scan_channel);
            } else {
                LOG_E("Pairing request failed on channel %d: 0x%x", g_espnow_ctx.scan_channel, result);
            }

            g_espnow_ctx.last_request_time = current_time;
            g_espnow_ctx.state = PAIRING_STATE_WAITING;
            break;
        }

        case PAIRING_STATE_WAITING:
        {
            // 等待响应超时
            if (current_time - g_espnow_ctx.last_request_time > CONFIG_PAIRING_RESPONSE_TIMEOUT_MS) {
                // 超时，切换到下一个信道
                g_espnow_ctx.scan_channel++;

                if (g_espnow_ctx.scan_channel > CONFIG_ESPNOW_MAX_CHANNEL) {
                    // 所有信道扫描完毕
                    g_espnow_ctx.scan_channel = 1;
                    g_espnow_ctx.retry_count++;

                    if (g_espnow_ctx.retry_count >= CONFIG_PAIRING_MAX_RETRY) {
                        LOG_E("Pairing timeout after %d retries", CONFIG_PAIRING_MAX_RETRY);
                        g_espnow_ctx.state = PAIRING_STATE_TIMEOUT;

                        // 进入深度睡眠（可选）
                        // power_mgmt_enter_deep_sleep(false);
                        return;
                    }
                }

                g_espnow_ctx.state = PAIRING_STATE_REQUESTING;
            }
            break;
        }

        default:
            break;
    }
}

/**
 * @brief 发送灯光控制命令
 */
bool espnow_ctrl_send_command(command_type_t cmd, const lamp_state_t *state) {
    if (!g_espnow_ctx.is_paired) {
        LOG_W("Not paired, cannot send command");
        return false;
    }

    data_message_t data_msg = {0};
    data_msg.msg_type = MSG_TYPE_DATA;
    data_msg.device_id = DEVICE_ID_CONTROLLER;

    // 填入发送方真实MAC地址（因为ESP-NOW源MAC可能是临时的）
    WiFi.macAddress(data_msg.sender_mac);

    data_msg.command = cmd;
    data_msg.seq_num = g_espnow_ctx.seq_num++;

    if (state != NULL) {
        memcpy(&data_msg.lamp_state, state, sizeof(lamp_state_t));
    }

    // 带重试的发送
    for (int i = 0; i < CONFIG_ESPNOW_MAX_RETRY; i++) {
        esp_err_t result = esp_now_send(g_espnow_ctx.peer_mac, (uint8_t *)&data_msg, sizeof(data_msg));

        if (result == ESP_OK) {
            LOG_D("Command sent: %d, seq: %d", cmd, data_msg.seq_num);
            return true;
        }

        delay(CONFIG_ESPNOW_RETRY_DELAY_MS);
    }

    LOG_E("Send command failed after %d retries", CONFIG_ESPNOW_MAX_RETRY);
    return false;
}

// ==================== 状态查询函数 ====================

pairing_state_t espnow_ctrl_get_pairing_state(void) {
    return g_espnow_ctx.state;
}

bool espnow_ctrl_is_paired(void) {
    return g_espnow_ctx.is_paired;
}

void espnow_ctrl_clear_pairing(void) {
    LOG_I("Clearing pairing info...");

    // 删除peer
    if (g_espnow_ctx.is_paired) {
        esp_now_del_peer(g_espnow_ctx.peer_mac);
    }

    // 清除内存
    memset(&g_espnow_ctx, 0, sizeof(g_espnow_ctx));
    g_espnow_ctx.state = PAIRING_STATE_INIT;
    g_espnow_ctx.is_paired = false;

    LOG_I("Pairing cleared");
}

bool espnow_ctrl_get_peer_mac(uint8_t *mac_out) {
    if (!g_espnow_ctx.is_paired || mac_out == NULL) {
        return false;
    }
    memcpy(mac_out, g_espnow_ctx.peer_mac, 6);
    return true;
}

uint8_t espnow_ctrl_get_peer_channel(void) {
    return g_espnow_ctx.is_paired ? g_espnow_ctx.peer_channel : 0;
}
