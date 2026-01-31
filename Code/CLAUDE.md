# ESP-NOW 智能台灯项目开发规范

## 文档目的
本文档为后续使用Claude进行本项目开发的规范指南，确保开发的连续性和代码质量。

---

## 项目架构概览

### 系统组成
```
智能台灯系统
├── rotary_controller (控制器)
│   ├── 硬件: ESP8684-MINI-1U + 旋转编码器 + 按键
│   ├── 供电: 电池供电（需要低功耗优化）
│   └── 功能: 用户交互、ESP-NOW发送、深度睡眠
│
└── Lamp_Driver (驱动器)
    ├── 硬件: ESP8684-MINI-1U + 双通道LED（5000K + 2700K）
    ├── 供电: 市电供电
    └── 功能: ESP-NOW接收、LEDC控制、配对响应
```

### 通信协议
- **协议**: ESP-NOW（点对点，无需路由器）
- **方向**: 控制器 → 驱动器（主动发送）
- **响应**: 驱动器 → 控制器（确认状态）

---

## 代码风格规范

### 命名约定

#### 1. 文件命名
- 源文件：小写蛇形，如 `esp_now_ctrl.cpp`
- 头文件：小写蛇形，如 `led_controller.h`
- 配置文件：小写蛇形，如 `config.h`

#### 2. 变量命名
```cpp
// 全局配置常量：大写蛇形 + CONFIG_ 前缀
#define CONFIG_MAX_BRIGHTNESS 511
#define CONFIG_SLEEP_TIMEOUT_MS 5000

// 模块级常量：大写蛇形 + 模块前缀
#define ESPNOW_MAX_RETRY 3
#define LED_FADE_TIME_MS 2000

// 全局变量：小写蛇形 + g_ 前缀（尽量避免）
static lamp_state_t g_lamp_state;

// 局部变量：小写蛇形
int retry_count = 0;
uint8_t channel_index = 0;

// RTC保留变量：RTC_DATA_ATTR + 小写蛇形
RTC_DATA_ATTR static bool rtc_is_paired = false;
```

#### 3. 函数命名
```cpp
// 模块公开函数：模块前缀 + 小写蛇形
void espnow_init_controller(void);
bool led_set_brightness(uint16_t brightness);

// 模块私有函数：static + 模块前缀 + 小写蛇形
static void espnow_on_data_sent(const uint8_t *mac, esp_now_send_status_t status);

// 回调函数：on_ 前缀 + 事件描述
static void on_encoder_rotate(int direction);
```

#### 4. 类型命名
```cpp
// 结构体：小写蛇形 + _t 后缀
typedef struct {
    bool is_on;
    uint16_t brightness;
} lamp_state_t;

// 枚举类型：小写蛇形 + _t 后缀
typedef enum {
    PAIRING_STATE_IDLE,
    PAIRING_STATE_SCANNING,
    PAIRING_STATE_PAIRED
} pairing_state_t;

// 枚举值：大写蛇形 + 类型前缀
enum {
    CMD_SET_STATE,
    CMD_POWER_OFF
} command_type_t;
```

### 代码组织

#### 头文件结构
```cpp
#pragma once

// 1. 标准库头文件
#include <stdint.h>
#include <stdbool.h>

// 2. ESP-IDF头文件
#include <esp_now.h>
#include <esp_wifi.h>

// 3. Arduino头文件
#include "Arduino.h"

// 4. 第三方库头文件
#include "AiEsp32RotaryEncoder.h"

// 5. 项目头文件
#include "config.h"

// 6. 常量定义
#define MODULE_MAX_VALUE 100

// 7. 类型定义
typedef struct { ... } my_type_t;

// 8. 函数声明
void module_init(void);
```

#### 源文件结构
```cpp
// 1. 头文件包含
#include "module_name.h"

// 2. 私有常量定义
#define PRIVATE_CONSTANT 42

// 3. 私有类型定义
typedef struct { ... } private_type_t;

// 4. 模块级静态变量
static int module_state = 0;

// 5. 私有函数声明
static void private_function(void);

// 6. 公开函数实现
void module_init(void) {
    // 实现
}

// 7. 私有函数实现
static void private_function(void) {
    // 实现
}
```

---

## 模块设计规范

### 模块职责划分

#### 控制器模块
```
rotary_controller/main/
├── main.cpp              - 主程序：setup()、loop()、任务调度
├── config.h              - 配置定义：GPIO、超时、PWM参数
├── lamp_state.h          - 状态定义：lamp_state_t、命令枚举
├── esp_now_ctrl.cpp/h    - ESP-NOW通信：初始化、配对、发送
├── encoder_handler.cpp/h - 编码器处理：中断、事件解析、按键
└── power_mgmt.cpp/h      - 电源管理：睡眠、唤醒、NVS、RTC
```

#### 驱动器模块
```
Lamp_Driver/main/
├── main.cpp              - 主程序：setup()、loop()
├── config.h              - 配置定义：GPIO、PWM参数
├── lamp_state.h          - 状态定义（与控制器共享）
├── esp_now_driver.cpp/h  - ESP-NOW通信：接收、配对响应
└── led_controller.cpp/h  - LED控制：LEDC初始化、淡入淡出
```

### 模块接口设计原则

#### 1. 初始化函数
```cpp
// 返回bool表示成功/失败
bool module_init(void);

// 示例
bool espnow_init_controller(void) {
    if (esp_now_init() != ESP_OK) {
        return false;
    }
    esp_now_register_send_cb(on_data_sent);
    return true;
}
```

#### 2. 状态查询函数
```cpp
// 使用is_/has_/get_前缀
bool module_is_ready(void);
int module_get_status(void);
```

#### 3. 操作函数
```cpp
// 返回值表示操作结果
bool module_do_action(param_t param);

// 示例
bool led_set_state(const lamp_state_t *state) {
    if (state == NULL) return false;
    led_fade_to_duty(state->duty_ch0, state->duty_ch1);
    return true;
}
```

---

## 深度睡眠开发规范

### RTC内存使用
```cpp
// 使用RTC_DATA_ATTR保存睡眠期间需要保留的数据
RTC_DATA_ATTR static bool rtc_is_paired = false;
RTC_DATA_ATTR static uint8_t rtc_peer_mac[6];
RTC_DATA_ATTR static uint8_t rtc_peer_channel;
RTC_DATA_ATTR static lamp_state_t rtc_lamp_state;

// RTC内存限制：ESP32-C2仅有8KB，谨慎使用
```

### 睡眠前检查清单
1. 保存必要状态到RTC内存
2. 保存配对信息到NVS（仅变化时）
3. 配置GPIO唤醒源
4. 打印日志（便于调试）
5. 调用`esp_deep_sleep_start()`

### 唤醒后恢复清单
1. 检查唤醒原因（`esp_sleep_get_wakeup_cause()`）
2. 从RTC内存恢复状态
3. 快速初始化WiFi和ESP-NOW（跳过扫描）
4. 处理唤醒事件
5. 重置睡眠计时器

---

## ESP-NOW通信规范

### 消息结构设计
```cpp
// 消息头：所有消息共享
typedef struct {
    uint8_t msg_type;    // PAIRING / DATA
    uint8_t device_id;   // 设备ID
    uint8_t seq_num;     // 序列号（检测丢包）
} __attribute__((packed)) msg_header_t;

// 数据消息
typedef struct {
    msg_header_t header;
    uint8_t command;     // CMD_SET_STATE / CMD_POWER_OFF
    lamp_state_t state;
} __attribute__((packed)) data_msg_t;

// 确保结构体对齐，避免不同设备解析错误
```

### 发送重试策略
```cpp
#define ESPNOW_MAX_RETRY 3
#define ESPNOW_RETRY_DELAY_MS 50

bool espnow_send_with_retry(const uint8_t *mac, const void *data, size_t len) {
    for (int i = 0; i < ESPNOW_MAX_RETRY; i++) {
        if (esp_now_send(mac, (uint8_t*)data, len) == ESP_OK) {
            return true;
        }
        delay(ESPNOW_RETRY_DELAY_MS);
    }
    return false;
}
```

### 错误处理
```cpp
// 记录错误到ERROR.md
void log_espnow_error(esp_err_t err, const char *context) {
    Serial.printf("[ERROR] ESP-NOW %s failed: 0x%x\n", context, err);
    // TODO: 记录到ERROR.md
}
```

---

## LED控制规范

### LEDC初始化
```cpp
bool led_init(void) {
    // 使用XTAL时钟源（更稳定）
    if (!ledcSetClockSource(LEDC_USE_XTAL_CLK)) {
        return false;
    }

    ledcAttach(CONFIG_LED_CH0_PIN, CONFIG_PWM_FREQ, CONFIG_PWM_RES);
    ledcAttach(CONFIG_LED_CH1_PIN, CONFIG_PWM_FREQ, CONFIG_PWM_RES);

    return true;
}
```

### 淡入淡出控制
```cpp
void led_fade_to_state(const lamp_state_t *target, uint32_t fade_time_ms) {
    ledcFade(CONFIG_LED_CH0_PIN,
             current_state.duty_ch0,
             target->duty_ch0,
             fade_time_ms);
    ledcFade(CONFIG_LED_CH1_PIN,
             current_state.duty_ch1,
             target->duty_ch1,
             fade_time_ms);

    // 更新当前状态
    current_state = *target;
}
```

### 亮度和色温计算
```cpp
// 亮度：0-511 (2^9 - 1)
// 色温：0.0-1.0 (0=2700K暖白, 1=5000K冷白)
void calculate_duty_from_state(const lamp_state_t *state,
                                 uint16_t *duty_ch0,
                                 uint16_t *duty_ch1) {
    if (!state->is_on) {
        *duty_ch0 = 0;
        *duty_ch1 = 0;
        return;
    }

    // 总亮度按色温分配到两个通道
    *duty_ch0 = state->brightness * state->temperature;        // 冷白
    *duty_ch1 = state->brightness * (1.0 - state->temperature); // 暖白
}
```

---

## 编码器处理规范

### 中断处理
```cpp
// ISR必须使用IRAM_ATTR（快速执行）
void IRAM_ATTR encoder_isr(void) {
    rotary_encoder.readEncoder_ISR();
}

// ISR中不做复杂逻辑，仅设置标志位
volatile bool encoder_changed = false;

void IRAM_ATTR on_encoder_change(void) {
    encoder_changed = true;
}
```

### 事件处理状态机
```cpp
typedef enum {
    ENCODER_MODE_IDLE,
    ENCODER_MODE_BRIGHTNESS,  // 调节亮度
    ENCODER_MODE_TEMPERATURE  // 调节色温（按住按键时）
} encoder_mode_t;

void encoder_process_event(encoder_event_t event) {
    static encoder_mode_t mode = ENCODER_MODE_IDLE;

    switch (event) {
        case ENCODER_EVENT_BTN_PRESS:
            mode = ENCODER_MODE_TEMPERATURE;
            break;
        case ENCODER_EVENT_BTN_RELEASE:
            mode = ENCODER_MODE_BRIGHTNESS;
            break;
        case ENCODER_EVENT_ROTATE_CW:
            if (mode == ENCODER_MODE_BRIGHTNESS) {
                increase_brightness();
            } else {
                increase_temperature();
            }
            break;
        // ...
    }
}
```

---

## NVS存储规范

### 命名空间
```cpp
#define NVS_NAMESPACE "lamp_ctrl"  // 控制器
#define NVS_NAMESPACE "lamp_drv"   // 驱动器
```

### 键名约定
```cpp
// 配对信息
#define NVS_KEY_PEER_MAC     "peer_mac"
#define NVS_KEY_PEER_CHANNEL "peer_ch"
#define NVS_KEY_IS_PAIRED    "is_paired"

// 灯光状态
#define NVS_KEY_LAMP_STATE   "lamp_state"
#define NVS_KEY_BRIGHTNESS   "brightness"
#define NVS_KEY_TEMPERATURE  "temp"
```

### 读写封装
```cpp
bool nvs_save_lamp_state(const lamp_state_t *state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        return false;
    }

    prefs.putBytes(NVS_KEY_LAMP_STATE, state, sizeof(lamp_state_t));
    prefs.end();
    return true;
}

bool nvs_load_lamp_state(lamp_state_t *state) {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return false;
    }

    size_t len = prefs.getBytes(NVS_KEY_LAMP_STATE, state, sizeof(lamp_state_t));
    prefs.end();

    return (len == sizeof(lamp_state_t));
}
```

---

## 调试与日志规范

### 日志级别
```cpp
#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARN    1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3

#ifndef CONFIG_LOG_LEVEL
#define CONFIG_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

#define LOG_E(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) if (CONFIG_LOG_LEVEL >= LOG_LEVEL_DEBUG) \
                            Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
```

### 关键节点日志
```cpp
// 启动日志
void print_startup_info(void) {
    LOG_I("=== Smart Lamp Controller v1.0 ===");
    LOG_I("MAC Address: %s", WiFi.macAddress().c_str());
    LOG_I("Wakeup reason: %d", esp_sleep_get_wakeup_cause());
}

// 配对日志
LOG_I("Pairing request sent to channel %d", channel);
LOG_I("Pairing success with %02x:%02x:...", mac[0], mac[1]);

// 控制日志
LOG_D("Brightness: %d, Temperature: %.2f", state.brightness, state.temperature);

// 睡眠日志
LOG_I("Entering deep sleep after %d ms idle", idle_time);
```

---

## CMake配置规范

### 组件依赖声明
```cmake
# rotary_controller/components/CMakelists.txt
idf_component_register(
    SRCS
        "esp_now_ctrl.cpp"
        "encoder_handler.cpp"
        "power_mgmt.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        arduino
        nvs_flash
)
```

### 编译选项
```cmake
# 优化级别
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)

# 警告级别
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
```

---

## 错误处理规范

### 错误码定义
```cpp
typedef enum {
    ERR_OK = 0,
    ERR_ESPNOW_INIT_FAILED,
    ERR_ESPNOW_SEND_FAILED,
    ERR_PAIRING_TIMEOUT,
    ERR_NVS_READ_FAILED,
    ERR_LED_INIT_FAILED
} error_code_t;
```

### 错误记录
```cpp
void log_error_to_file(error_code_t err, const char *detail) {
    // 记录到ERROR.md，格式：
    // ## [时间戳] 错误码
    // **详情**: ...
    // **解决方案**: ...
}
```

---

## 性能优化指南

### 1. 启动优化
- 使用RTC内存避免NVS读取
- WiFi使用固定信道避免扫描
- 跳过不必要的初始化

### 2. 功耗优化
- 控制器：深度睡眠功耗<10uA
- 驱动器：WiFi省电模式（ESP-NOW兼容）
- LED：PWM频率20KHz（平衡效率和噪音）

### 3. 响应优化
- ESP-NOW单播延迟<10ms
- 编码器中断响应<1ms
- LED淡入淡出帧率>60fps

---

## 代码审查检查清单

### 提交前自查
- [ ] 无硬编码魔数（使用宏定义）
- [ ] 无全局可变变量（除必要情况）
- [ ] 无未使用的代码和注释
- [ ] 函数长度<50行（复杂函数拆分）
- [ ] 变量命名清晰（避免a/b/tmp）
- [ ] 添加必要注释（复杂逻辑）
- [ ] 错误处理完整（检查返回值）
- [ ] 日志输出适当（关键节点）

### 功能验证
- [ ] 编译无警告
- [ ] 逻辑正确性检查
- [ ] 边界条件考虑
- [ ] 资源释放检查（内存、NVS句柄）

---

## 持续开发流程

### 每次会话开始
1. 读取 `ACTION.md` 了解进度
2. 读取 `ERROR.md` 了解已知问题
3. 读取 `PLAN.md` 确认当前阶段
4. 读取相关源文件

### 每次会话结束
1. 更新 `ACTION.md` 记录操作
2. 更新 `ERROR.md` 记录错误
3. 提交代码变更说明

---

## 附录：常用参考

### ESP32-C2 GPIO限制
- GPIO0-5: 可用（本项目使用0-5）
- GPIO8-9: FLASH保留（禁用）
- GPIO18-19: USB (ESP8684-MINI-1U不可用)

### ESP-NOW限制
- 加密peer: 最多6个
- 非加密peer: 最多20个
- 单包最大250字节

### LEDC限制
- 6个通道（本项目使用2个）
- PWM频率范围: 1Hz - 40MHz
- 分辨率: 最高14位（@1KHz）

---

**文档版本**: 1.0
**创建日期**: 2026-01-30
**维护者**: Claude (Embedded Expert)
**适用范围**: ESP-NOW智能台灯项目全周期开发
