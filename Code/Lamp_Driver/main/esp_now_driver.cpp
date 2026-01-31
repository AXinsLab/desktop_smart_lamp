/**
 * @file esp_now_driver.cpp
 * @brief 驱动器ESP-NOW通信模块实现
 */

#include "esp_now_driver.h"
#include "led_controller.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>

// ==================== 模块私有变量 ====================
#define MAX_CONTROLLERS 3  // 最多支持3个控制器

typedef struct {
    uint8_t mac[6];
    uint8_t channel;
    bool is_active;
} controller_info_t;

static controller_info_t g_controllers[MAX_CONTROLLERS] = {0};
static uint8_t g_controller_count = 0;
static esp_now_peer_info_t g_peer_info = {0};

// ==================== 辅助函数 ====================

/**
 * @brief 打印MAC地址
 */
static void print_mac(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/**
 * @brief 比较两个MAC地址
 */
static bool mac_equal(const uint8_t *mac1, const uint8_t *mac2) {
    return memcmp(mac1, mac2, 6) == 0;
}

/**
 * @brief 查找控制器索引
 */
static int find_controller_index(const uint8_t *mac) {
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        if (g_controllers[i].is_active && mac_equal(g_controllers[i].mac, mac)) {
            return i;
        }
    }
    return -1;
}

// ==================== ESP-NOW回调函数 ====================

/**
 * @brief 发送回调
 */
void espnow_driver_on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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
void espnow_driver_on_data_recv(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (data == NULL || len < 1) {
        return;
    }

    uint8_t msg_type = data[0];

    // 处理配对请求
    if (msg_type == MSG_TYPE_PAIRING) {
        if (len < sizeof(pairing_message_t)) {
            LOG_W("Invalid pairing message size: %d", len);
            return;
        }

        pairing_message_t *pairing_msg = (pairing_message_t *)data;

        LOG_D("Pairing message received: type=%d, device_id=%d",
              pairing_msg->msg_type, pairing_msg->device_id);

        // 检查是否来自控制器
        if (pairing_msg->device_id == DEVICE_ID_CONTROLLER) {
            LOG_I("=== Pairing Request Received ===");

            // 关键修复：ESP-NOW广播时回调的MAC可能是临时MAC
            // 应该使用消息体中的真实MAC地址
            LOG_I("Controller MAC (from ESP-NOW callback): ");
            print_mac(mac_addr);
            Serial.printf(" (可能是临时MAC)");
            Serial.printf("\nController MAC (from message body): ");
            print_mac(pairing_msg->mac_addr);
            Serial.printf(" (真实MAC)");
            Serial.printf("\nController Channel: %d\n", pairing_msg->channel);

            // 使用消息体中的真实MAC地址添加控制器和发送响应
            if (espnow_driver_add_controller(pairing_msg->mac_addr, pairing_msg->channel)) {
                LOG_I("Controller added successfully, sending response...");
                // 发送配对响应（使用消息体中的真实MAC）
                if (espnow_driver_send_pairing_response(pairing_msg->mac_addr, pairing_msg->channel)) {
                    LOG_I("Pairing response sent successfully");
                } else {
                    LOG_E("Failed to send pairing response");
                }
            } else {
                LOG_E("Failed to add controller");
            }
        } else {
            LOG_W("Pairing request from unknown device type: %d", pairing_msg->device_id);
        }
    }
    // 处理数据命令
    else if (msg_type == MSG_TYPE_DATA) {
        if (len < sizeof(data_message_t)) {
            LOG_W("Invalid data message size: %d", len);
            return;
        }

        data_message_t *data_msg = (data_message_t *)data;

        // 关键修复：使用消息体中的sender_mac验证，而不是回调的mac_addr
        // 因为ESP-NOW的源MAC可能是临时/随机MAC
        LOG_D("Data from ESP-NOW callback MAC: ");
        print_mac(mac_addr);
        Serial.printf(", sender_mac in message: ");
        print_mac(data_msg->sender_mac);
        Serial.println();

        // 检查是否来自已配对的控制器（使用消息体中的真实MAC）
        if (find_controller_index(data_msg->sender_mac) >= 0) {
            LOG_D("Command received: %d, seq: %d", data_msg->command, data_msg->seq_num);

            // 处理命令
            espnow_driver_handle_command(
                (command_type_t)data_msg->command,
                &data_msg->lamp_state,
                data_msg->sender_mac  // 使用消息体中的真实MAC
            );
        } else {
            LOG_W("Command from unpaired controller (callback MAC): ");
            print_mac(mac_addr);
            Serial.printf(", sender_mac: ");
            print_mac(data_msg->sender_mac);
            Serial.println();
        }
    }
}

// ==================== 公开函数实现 ====================

/**
 * @brief 初始化ESP-NOW驱动器
 */
bool espnow_driver_init(void) {
    LOG_I("Initializing ESP-NOW driver...");

    // 初始化WiFi（STA模式）
    WiFi.mode(WIFI_STA);
    WiFi.STA.begin();

    // 打印MAC地址
    LOG_I("Driver MAC: %s", WiFi.macAddress().c_str());

    // 获取并打印信道
    uint8_t channel = WiFi.channel();
    LOG_I("WiFi channel: %d", channel);

    // 初始化ESP-NOW
    if (esp_now_init() != ESP_OK) {
        LOG_E("ESP-NOW init failed");
        return false;
    }

    // 注册回调
    esp_now_register_send_cb(espnow_driver_on_data_sent);
    esp_now_register_recv_cb(esp_now_recv_cb_t(espnow_driver_on_data_recv));

    LOG_I("ESP-NOW driver initialized successfully");
    return true;
}

/**
 * @brief 添加控制器
 */
bool espnow_driver_add_controller(const uint8_t *controller_mac, uint8_t channel) {
    if (controller_mac == NULL) {
        return false;
    }

    LOG_D("Adding controller: ");
    print_mac(controller_mac);
    Serial.printf(", channel=%d\n", channel);

    // 检查是否已存在
    int index = find_controller_index(controller_mac);
    if (index >= 0) {
        LOG_I("Controller already paired");
        return true;
    }

    // 查找空位
    index = -1;
    for (int i = 0; i < MAX_CONTROLLERS; i++) {
        if (!g_controllers[i].is_active) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        LOG_E("Controller list full");
        return false;
    }

    // 获取当前驱动器信道
    uint8_t driver_channel = WiFi.channel();
    LOG_D("Driver current channel: %d, using channel: %d for peer", driver_channel, channel);

    // 添加为ESP-NOW peer
    esp_now_del_peer(controller_mac);  // 删除旧的（如果存在）

    memset(&g_peer_info, 0, sizeof(esp_now_peer_info_t));
    memcpy(g_peer_info.peer_addr, controller_mac, 6);
    // 使用控制器的信道（ESP-NOW允许跨信道通信）
    g_peer_info.channel = channel;
    g_peer_info.encrypt = false;

    esp_err_t result = esp_now_add_peer(&g_peer_info);
    if (result != ESP_OK) {
        LOG_E("Failed to add peer: 0x%x", result);
        return false;
    }

    // 保存到控制器列表
    memcpy(g_controllers[index].mac, controller_mac, 6);
    g_controllers[index].channel = channel;
    g_controllers[index].is_active = true;
    g_controller_count++;

    LOG_I("Controller added successfully [%d/%d]", g_controller_count, MAX_CONTROLLERS);
    LOG_I("  MAC: ");
    print_mac(controller_mac);
    Serial.printf("\n  Channel: %d\n", channel);

    return true;
}

/**
 * @brief 发送配对响应
 */
bool espnow_driver_send_pairing_response(const uint8_t *controller_mac, uint8_t controller_channel) {
    if (controller_mac == NULL) {
        return false;
    }

    pairing_message_t pairing_msg = {0};
    pairing_msg.msg_type = MSG_TYPE_PAIRING;
    pairing_msg.device_id = DEVICE_ID_DRIVER;
    WiFi.macAddress(pairing_msg.mac_addr);
    pairing_msg.channel = WiFi.channel();
    pairing_msg.timestamp = millis();

    LOG_D("Sending pairing response: driver_channel=%d", pairing_msg.channel);

    // 多次重试发送配对响应（提高可靠性）
    bool send_success = false;
    for (int i = 0; i < 3; i++) {
        esp_err_t result = esp_now_send(controller_mac, (uint8_t *)&pairing_msg, sizeof(pairing_msg));
        if (result == ESP_OK) {
            send_success = true;
            break;
        }
        LOG_W("Pairing response send failed (attempt %d/3): 0x%x", i + 1, result);
        delay(100);
    }

    if (!send_success) {
        LOG_E("Failed to send pairing response after 3 attempts");
        return false;
    }

    LOG_I("Pairing response sent to ");
    print_mac(controller_mac);
    Serial.println();

    return true;
}

/**
 * @brief 发送状态响应
 */
bool espnow_driver_send_state_response(const uint8_t *controller_mac, const lamp_state_t *state) {
    if (controller_mac == NULL || state == NULL) {
        return false;
    }

    data_message_t data_msg = {0};
    data_msg.msg_type = MSG_TYPE_DATA;
    data_msg.device_id = DEVICE_ID_DRIVER;
    data_msg.command = CMD_STATE_RESPONSE;
    data_msg.seq_num = 0;  // 响应不需要序列号
    memcpy(&data_msg.lamp_state, state, sizeof(lamp_state_t));

    esp_err_t result = esp_now_send(controller_mac, (uint8_t *)&data_msg, sizeof(data_msg));
    if (result != ESP_OK) {
        LOG_E("Failed to send state response: 0x%x", result);
        return false;
    }

    LOG_D("State response sent");
    return true;
}

/**
 * @brief 检查是否已配对
 */
bool espnow_driver_is_paired_with(const uint8_t *controller_mac) {
    if (controller_mac == NULL) {
        return false;
    }
    return (find_controller_index(controller_mac) >= 0);
}

/**
 * @brief 处理接收到的命令
 */
void espnow_driver_handle_command(command_type_t cmd, const lamp_state_t *state, const uint8_t *controller_mac) {
    lamp_state_t current_state;

    switch (cmd) {
        case CMD_POWER_ON:
            LOG_I("Command: POWER ON");
            if (state != NULL) {
                led_ctrl_power_on(state);
            } else {
                led_ctrl_power_on(NULL);  // 使用上次保存的状态
            }
            break;

        case CMD_POWER_OFF:
            LOG_I("Command: POWER OFF");
            led_ctrl_power_off();
            break;

        case CMD_SET_LAMP_STATE:
            LOG_I("Command: SET STATE (brightness=%u, temp=%.2f)",
                  state->brightness, state->temperature);
            led_ctrl_set_state(state);
            break;

        default:
            LOG_W("Unknown command: %d", cmd);
            return;
    }

    // 发送状态响应给控制器
    if (led_ctrl_get_current_state(&current_state)) {
        espnow_driver_send_state_response(controller_mac, &current_state);
    }
}
