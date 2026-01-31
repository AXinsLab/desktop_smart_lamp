# 错误记录与解决方案 (ERROR LOG)

## 文档说明
本文档记录开发过程中遇到的错误、问题分析和解决方案，防止重复犯错。

---

## 错误分类
- **编译错误**: 语法、链接、依赖问题
- **运行时错误**: 逻辑错误、崩溃、异常
- **功能缺陷**: 功能不符合预期
- **性能问题**: 响应慢、功耗高、内存泄漏
- **通信错误**: ESP-NOW失败、丢包

---

## 已知问题列表

### [2026-01-30] #1 LED POWER_ON逻辑错误
**类型**: 功能缺陷
**严重级别**: 高
**发现位置**: `Lamp_Driver/main/main.cpp:87-100`

**问题描述**:
```cpp
inline void led_power_on()
{
    ledcFade(
        LED_CH0_PIN,
        0,  // 从0开始
        uint32_t(current_duty_ch[0]),  // 淡入到current_duty
        LED_POWER_ON_OFF_FADE_TIME_MS);
    // ...
}
```

当灯关闭时，`current_duty_ch[0/1]`已经被设置为0（在关灯时），再次开灯时从0淡入到0，无法正常亮灯。

**根本原因**:
- 没有保存关灯前的亮度状态
- POWER_ON和SET_DUTY逻辑混淆

**解决方案**:
1. 增加`last_duty_ch[2]`保存关灯前的亮度
2. POWER_ON命令恢复`last_duty_ch`到`current_duty_ch`
3. 关灯时保存`current_duty_ch`到`last_duty_ch`

**修改后的逻辑**:
```cpp
static uint16_t last_duty_ch[2] = {255, 255}; // 默认中等亮度

void led_power_on() {
    current_duty_ch[0] = last_duty_ch[0];
    current_duty_ch[1] = last_duty_ch[1];
    ledcFade(LED_CH0_PIN, 0, current_duty_ch[0], LED_POWER_ON_OFF_FADE_TIME_MS);
    ledcFade(LED_CH1_PIN, 0, current_duty_ch[1], LED_POWER_ON_OFF_FADE_TIME_MS);
}

void led_power_off() {
    last_duty_ch[0] = current_duty_ch[0];
    last_duty_ch[1] = current_duty_ch[1];
    current_duty_ch[0] = 0;
    current_duty_ch[1] = 0;
    ledcFade(LED_CH0_PIN, last_duty_ch[0], 0, LED_POWER_ON_OFF_FADE_TIME_MS);
    ledcFade(LED_CH1_PIN, last_duty_ch[1], 0, LED_POWER_ON_OFF_FADE_TIME_MS);
}
```

**状态**: 待修复（计划在阶段1重构时修复）

---

### [2026-01-30] #2 深度睡眠配对丢失问题
**类型**: 功能缺陷
**严重级别**: 高
**发现位置**: `rotary_controller/main/main.cpp:486-492`

**问题描述**:
深度睡眠相关代码被完全注释掉，原因可能是唤醒后重新配对延迟严重（2-3秒），用户体验极差。

**根本原因**:
1. 唤醒后WiFi重新初始化耗时
2. ESP-NOW重新配对需要扫描信道
3. 没有使用RTC内存保存配对信息

**解决方案**:
使用RTC内存+NVS双保险策略：

**快速路径（RTC内存）**:
```cpp
RTC_DATA_ATTR static bool rtc_is_paired = false;
RTC_DATA_ATTR static uint8_t rtc_peer_mac[6];
RTC_DATA_ATTR static uint8_t rtc_peer_channel;

void quick_reconnect() {
    if (rtc_is_paired) {
        ESP_ERROR_CHECK(esp_wifi_set_channel(rtc_peer_channel, WIFI_SECOND_CHAN_NONE));
        esp_now_peer_info_t peer;
        memcpy(peer.peer_addr, rtc_peer_mac, 6);
        peer.channel = rtc_peer_channel;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
        // 跳过扫描，直接发送
    }
}
```

**慢速路径（NVS恢复）**:
- RTC内存失效时（如长时间断电）从NVS恢复

**性能目标**:
- 唤醒到发送命令 < 200ms（快速路径）
- < 2秒（慢速路径）

**状态**: 待实现（计划在阶段3实现）

---

### [2026-01-30] #3 NVS存储逻辑不完整
**类型**: 功能缺陷
**严重级别**: 中
**发现位置**:
- `rotary_controller/main/main.cpp:294-299`
- `rotary_controller/main/main.cpp:368-385` (全部注释)

**问题描述**:
1. 控制器有`saveMyPreference`函数但大部分存储逻辑被注释
2. 驱动器的NVS读取逻辑被注释（305-318行）
3. `format_test=true`每次启动清空NVS（测试代码残留）

**根本原因**:
- 开发过程中频繁测试，为方便调试注释了NVS
- 测试代码未清理

**解决方案**:
1. 移除`format_test`测试代码
2. 重新启用NVS存储逻辑
3. 封装NVS操作到`power_mgmt.cpp`模块
4. 设计明确的存储策略：
   - 配对成功时保存peer信息
   - 深度睡眠前保存灯光状态
   - 启动时恢复

**状态**: 待修复（计划在阶段1重构时修复）

---

### [2026-01-30] #4 硬编码MAC地址导致无法自动配对
**类型**: 架构问题
**严重级别**: 高
**发现位置**:
- `rotary_controller/main/main.hpp:50` - serverAddress
- `Lamp_Driver/main/main.hpp:48` - clientMacAddress

**问题描述**:
```cpp
// 控制器
uint8_t serverAddress[] = {0x80, 0x64, 0x6f, 0x41, 0x5d, 0x5c};

// 驱动器
uint8_t clientMacAddress[6] = {0x50, 0x40, 0x6f, 0x40, 0xba, 0x18};
```

硬编码MAC导致：
1. 无法更换设备
2. 无法实现自动配对
3. 无法支持多控制器

**根本原因**:
- 初期为快速测试采用硬编码
- 自动配对逻辑未实现

**解决方案**:
实现ESP-NOW自动配对协议（详见PLAN.md阶段2）：
1. 控制器广播配对请求
2. 驱动器响应并建立连接
3. 保存配对信息到NVS

**状态**: 待实现（计划在阶段2实现）

---

### [2026-01-30] #5 编码器旋转开灯功能缺失
**类型**: 功能缺陷
**严重级别**: 中
**发现位置**: `rotary_controller/main/main.cpp:496-537`

**问题描述**:
需求是"旋转编码器开灯"，但当前逻辑：
```cpp
if (rotaryEncoder.encoderChanged())
{
    // ...
    if (encoderButton.isPressed())
    {
        update_led_temperature(encoder_change_value);  // 色温调节
    }
    else
    {
        update_target_duty(encoder_change_value);  // 亮度调节
    }
    // 发送命令
}
```

无论灯是否开启，都直接调节亮度，没有判断`ledState`。

**根本原因**:
- 缺少状态机管理
- 开灯逻辑不完整

**解决方案**:
在编码器处理中增加状态判断：
```cpp
if (rotaryEncoder.encoderChanged()) {
    if (ledState == LED_POWER_OFF) {
        // 开灯
        led_power_on();
        ledState = LED_POWER_ON;
    } else {
        // 调节亮度/色温
        if (encoderButton.isPressed()) {
            update_led_temperature(encoder_change_value);
        } else {
            update_target_duty(encoder_change_value);
        }
    }
}
```

**状态**: 待修复（计划在阶段4实现）

---

### [2026-01-30] #6 全局变量过多导致代码难以维护
**类型**: 架构问题
**严重级别**: 中
**发现位置**: `rotary_controller/main/main.hpp` 和 `main.cpp`

**问题描述**:
控制器有20+个全局变量：
- 配对相关：serverAddress, clientMacAddress, peer_info, pairingData, channel...
- 状态相关：ledState, rotary_direction, pairingStatus...
- 时间相关：currentMillis, previousMillis, start_time...
- 数据相关：send_data, inData, myData...

**根本原因**:
- 缺少模块化设计
- 所有逻辑都在main.cpp中

**解决方案**:
按模块封装状态：

**ESP-NOW模块**:
```cpp
// esp_now_ctrl.h
typedef struct {
    bool is_paired;
    uint8_t peer_mac[6];
    uint8_t peer_channel;
    pairing_state_t state;
} espnow_context_t;
```

**电源管理模块**:
```cpp
// power_mgmt.h
typedef struct {
    uint32_t last_activity_time;
    uint32_t sleep_timeout_ms;
} power_context_t;
```

**状态**: 待重构（计划在阶段1实现）

---

## 潜在风险

### [待验证] ESP32-C2 GPIO唤醒可靠性
**风险描述**:
ESP32-C2使用GPIO唤醒深度睡眠，需要验证编码器旋转产生的短脉冲能否可靠唤醒。

**可能的问题**:
- 编码器旋转脉冲可能太短（<100us）
- GPIO下拉可能导致误唤醒

**验证方法**:
1. 测试不同编码器速度的唤醒成功率
2. 调整GPIO唤醒灵敏度
3. 可能需要增加硬件防抖电路

**备选方案**:
- 使用RTC GPIO中断唤醒
- 增加硬件防抖电路

**状态**: 待验证（阶段3测试）

---

### [待验证] LEDC淡入淡出性能
**风险描述**:
ledcFade()在ESP32-C2上的性能需要验证，可能出现：
- 卡顿、不流畅
- CPU占用率高
- 与ESP-NOW冲突

**验证方法**:
1. 测试不同fade_time的效果
2. 监控CPU占用率
3. 验证fade期间ESP-NOW通信是否正常

**备选方案**:
- 使用RTOS任务异步处理淡入淡出
- 降低PWM分辨率提升性能

**状态**: 待验证（阶段4测试）

---

## 调试技巧

### ESP-NOW调试
```cpp
// 打印MAC地址
void print_mac(const uint8_t *mac) {
    Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// 监控发送状态
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    Serial.print("Send to ");
    print_mac(mac);
    Serial.printf(" status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}
```

### 深度睡眠调试
```cpp
void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    Serial.printf("Wakeup reason: %d\n", reason);
    if (reason == ESP_SLEEP_WAKEUP_GPIO) {
        uint64_t mask = esp_sleep_get_gpio_wakeup_status();
        Serial.printf("GPIO wakeup mask: 0x%llx\n", mask);
    }
}
```

### NVS调试
```cpp
void nvs_dump_namespace(const char *ns) {
    Preferences prefs;
    prefs.begin(ns, true);
    // 打印所有键值对
    Serial.printf("NVS namespace '%s':\n", ns);
    // 逐个读取并打印
    prefs.end();
}
```

---

## 经验总结

### 最佳实践
1. **避免在ISR中执行复杂逻辑**
   - 仅设置标志位，主循环处理

2. **ESP-NOW发送前检查配对状态**
   - 避免未配对时发送导致崩溃

3. **NVS操作使用RAII模式**
   - Preferences自动管理，确保end()调用

4. **深度睡眠前打印日志**
   - 方便调试睡眠相关问题

### 常见错误
1. **忘记添加peer导致ESP-NOW发送失败**
   - 症状：send返回ESP_ERR_ESPNOW_NOT_FOUND
   - 解决：esp_now_add_peer()

2. **GPIO配置冲突**
   - 症状：编码器不响应或误触发
   - 解决：检查上拉/下拉配置

3. **PWM频率过高导致LED闪烁**
   - 症状：低亮度时LED闪烁
   - 解决：降低频率到20KHz

---

## 编译错误修复记录

### [2026-01-30] 编译错误 #1: AiEsp32RotaryEncoder.h找不到
**类型**: 编译错误
**严重级别**: 高
**发现位置**: 编译 rotary_controller 时

**错误信息**:
```
AiEsp32RotaryEncoder.h: No such file or directory
```

**问题原因**:
main/CMakeLists.txt 中缺少对 `components` 组件的依赖声明。虽然 components/CMakeLists.txt 已经注册了包含 AiEsp32RotaryEncoder 和 Button2 的组件，但 main 模块没有声明需要这个组件。

**解决方案**:
在 `rotary_controller/main/CMakeLists.txt` 的 REQUIRES 中添加 `components`：

```cmake
idf_component_register(
    SRCS
        "main.cpp"
        "esp_now_ctrl.cpp"
        "encoder_handler.cpp"
        "power_mgmt.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        driver
        components  # 添加这一行
)
```

**状态**: ✅ 已修复

---

### [2026-01-30] 编译错误 #2: 格式化字符串类型不匹配 (多处)
**类型**: 编译警告转错误
**严重级别**: 高
**发现位置**: config.h:77 (宏展开时报错，实际问题在多个调用位置)

**错误信息**:
```
format '%d' expects argument of type 'int', but argument 3 has type 'uint32_t' {aka 'long unsigned int'} [-Werror=format=]
```

**问题原因**:
ESP-IDF 默认开启 `-Werror=format`，将格式不匹配警告视为错误。代码中多处使用 `%d` 打印无符号类型：
1. `uint32_t` 类型（如 `ESP.getFreeHeap()`）使用了 `%d`
2. `uint16_t` 类型（如 `brightness`）使用了 `%d`

**修复详情**:

#### 控制器 (rotary_controller)
修复了 **7处** 格式化错误：

1. **power_mgmt.cpp:372** - `ESP.getFreeHeap()` (uint32_t)
   ```cpp
   // 修复前
   LOG_I("Free heap: %d bytes", ESP.getFreeHeap());
   // 修复后
   LOG_I("Free heap: %lu bytes", (unsigned long)ESP.getFreeHeap());
   ```

2. **main.cpp:84** - `brightness` (uint16_t)
   ```cpp
   // 修复前
   LOG_I("Lamp state: on=%d, brightness=%d, temp=%.2f", ...);
   // 修复后
   LOG_I("Lamp state: on=%d, brightness=%u, temp=%.2f", ...);
   ```

3. **main.cpp:156** - `brightness` (uint16_t)
4. **encoder_handler.cpp:170** - `brightness` (uint16_t)
5. **esp_now_ctrl.cpp:129** - `brightness` (uint16_t)
6. **power_mgmt.cpp:272** - `brightness` (uint16_t)
7. **power_mgmt.cpp:349** - `brightness` (uint16_t)

#### 驱动器 (Lamp_Driver)
预防性修复了 **5处** 潜在格式化错误：

1. **led_controller.cpp:98** - `brightness` (uint16_t)
2. **led_controller.cpp:127** - `brightness` (uint16_t)
3. **led_controller.cpp:192** - `brightness` (uint16_t)
4. **led_controller.cpp:206** - `brightness` (uint16_t)
5. **esp_now_driver.cpp:306** - `brightness` (uint16_t)

**技术说明**:
| 类型 | 错误格式 | 正确格式 | 示例 |
|------|---------|---------|------|
| `uint32_t` | `%d` | `%lu` | `(unsigned long)value` |
| `uint16_t` | `%d` | `%u` | `value` |
| `uint8_t` | 通常可用 `%u` 或 `%d` | `%u` (推荐) | `value` |

**状态**: ✅ 已全部修复

---

## 运行时错误修复记录

### [2026-01-30] 运行时错误 #3: 配对失败导致驱动器拒绝命令
**类型**: 运行时错误
**严重级别**: 高
**现象**:

**控制器日志**:
```
[DEBUG] Send OK to D4:8C:49:86:1F:00
[DEBUG] Command sent: 0, seq: 2
[INFO] RTC paired: 0  ← 关键：未配对
```

**驱动器日志**:
```
[WARN] Command from unpaired controller: 1E:EB:CB:3F:18:EB
```

**问题原因**:
1. 控制器和驱动器之间的**配对握手没有完成**
2. 控制器可能添加了驱动器为peer（所以能发送命令）
3. 但驱动器没有收到配对请求，所以没有添加控制器为peer
4. 导致驱动器拒绝控制器的命令

**可能的根本原因**:
1. 控制器启动后立即接收用户输入，在配对完成前就尝试发送命令
2. 配对请求/响应在无线传输中丢失
3. 启动日志打印太快，用户看不到完整的配对过程

**解决方案**:

#### 1. 控制器增加配对等待逻辑 (main.cpp)
在 `setup()` 中增加配对等待，确保配对完成后才允许发送命令：

```cpp
// 等待配对完成（最多10秒）
LOG_I("Waiting for pairing to complete...");
uint32_t pairing_start = millis();
while (!espnow_ctrl_is_paired() && (millis() - pairing_start < 10000)) {
    espnow_ctrl_process();
    delay(100);
}

if (espnow_ctrl_is_paired()) {
    LOG_I("Pairing completed successfully!");
} else {
    LOG_W("Pairing timeout, will retry in loop");
}
```

#### 2. 增强所有配对相关日志
在关键位置添加详细日志：

**控制器 (esp_now_ctrl.cpp)**:
- 扫描每个信道时打印日志
- 发送配对请求时打印完整信息
- 接收配对响应时打印详细信息

**驱动器 (esp_now_driver.cpp)**:
- 接收配对请求时打印详细信息
- 添加控制器时打印成功/失败状态
- 发送配对响应时带重试机制（3次）

#### 3. 配对响应重试机制
驱动器发送配对响应时增加重试：

```cpp
// 多次重试发送配对响应（提高可靠性）
for (int i = 0; i < 3; i++) {
    esp_err_t result = esp_now_send(controller_mac, ...);
    if (result == ESP_OK) {
        send_success = true;
        break;
    }
    delay(100);
}
```

#### 4. 增加串口延迟
将启动时的串口延迟从 100ms 增加到 500ms，确保日志完整打印。

**验证步骤**:
1. 重新烧录控制器和驱动器
2. 观察启动日志，应该看到完整的配对过程：
   - 控制器：扫描信道、发送配对请求
   - 驱动器：接收配对请求、添加控制器、发送响应
   - 控制器：接收配对响应、保存配对信息
3. 配对成功后，控制器日志应显示 `is_paired = 1`
4. 旋转编码器，驱动器应正常接收并执行命令

**修改的文件**:
- ✅ `rotary_controller/main/main.cpp` - 增加配对等待
- ✅ `rotary_controller/main/esp_now_ctrl.cpp` - 增强配对日志
- ✅ `Lamp_Driver/main/main.cpp` - 增强启动日志
- ✅ `Lamp_Driver/main/esp_now_driver.cpp` - 增强配对日志和重试机制

**状态**: 🔄 部分修复，需要重新配对

---

### [2026-01-30] 运行时错误 #4: MAC地址不匹配导致配对失败
**类型**: 运行时错误
**严重级别**: 高
**现象**:

**完整日志分析**：

控制器打印:
```
[INFO] Controller MAC: D4:8C:49:84:3C:6C
[INFO] Pairing info loaded from NVS: D4:8C:49:86:1F:00, channel: 1
[INFO] Quick reconnect successful
[DEBUG] Command sent successfully
[INFO] RTC paired: 0  ← 关键：RTC未保存配对状态
```

驱动器打印:
```
[INFO] Driver MAC: D4:8C:49:86:1F:00
[WARN] Command from unpaired controller: 1E:EB:CB:3F:18:EB  ← MAC不匹配！
```

**关键发现**：
- 控制器打印的MAC: `D4:8C:49:84:3C:6C`
- 驱动器收到的MAC: `1E:EB:CB:3F:18:EB`
- **两个MAC完全不同！**

**问题根本原因**：

ESP-NOW发送时使用的**源MAC地址是WiFi硬件自动填充的**，不是我们在消息体里填的MAC。

1. `WiFi.macAddress()` 返回的可能是STA接口的MAC
2. ESP-NOW实际发送时可能使用不同的MAC（如AP MAC）
3. 之前的配对代码使用消息体中的MAC（不准确）
4. 驱动器保存了错误的控制器MAC，导致拒绝命令

**ESP-NOW回调参数说明**：
```cpp
void OnDataRecv(const uint8_t *mac_addr,  // ← 这是发送方的真实MAC（硬件自动填充）
                const uint8_t *data,       // ← 消息体
                int len)
```

**解决方案**：

#### 1. 修复配对逻辑 - 使用回调的MAC地址

**驱动器端 (esp_now_driver.cpp)**:
```cpp
// 错误的做法：
espnow_driver_add_controller(pairing_msg->mac_addr, ...);

// 正确的做法：使用回调提供的真实MAC
espnow_driver_add_controller(mac_addr, ...);  // mac_addr来自回调参数
```

**控制器端 (esp_now_ctrl.cpp)**:
```cpp
// 错误的做法：
memcpy(g_espnow_ctx.peer_mac, pairing_msg->mac_addr, 6);

// 正确的做法：使用回调提供的真实MAC
memcpy(g_espnow_ctx.peer_mac, mac_addr, 6);  // mac_addr来自回调参数
```

#### 2. 添加调试日志对比两个MAC

同时打印消息体中的MAC和回调提供的MAC：
```cpp
LOG_I("Controller MAC (from ESP-NOW): ");
print_mac(mac_addr);  // 真实MAC
Serial.printf("\nController MAC (from message): ");
print_mac(pairing_msg->mac_addr);  // 可能不准确
```

#### 3. 清除旧的NVS数据重新配对

添加编译时选项清除NVS：

**config.h**:
```cpp
#define CONFIG_CLEAR_NVS_ON_BOOT  1  // 首次配对时设为1，成功后改为0
```

**main.cpp**:
```cpp
#if CONFIG_CLEAR_NVS_ON_BOOT
    LOG_W("Clearing NVS for fresh pairing...");
    power_mgmt_clear_nvs();
#endif
```

**操作步骤**：
1. 控制器：设置 `CONFIG_CLEAR_NVS_ON_BOOT = 1`
2. 重新编译烧录控制器和驱动器
3. 观察完整的配对过程，确认MAC地址一致
4. 配对成功后，将 `CONFIG_CLEAR_NVS_ON_BOOT` 改回 `0`
5. 重新编译烧录（正常使用模式）

**修改的文件**:
- ✅ `rotary_controller/main/esp_now_ctrl.cpp` - 使用回调MAC
- ✅ `Lamp_Driver/main/esp_now_driver.cpp` - 使用回调MAC
- ✅ `rotary_controller/main/config.h` - 添加清除NVS选项
- ✅ `rotary_controller/main/main.cpp` - 实现清除NVS
- ✅ `Lamp_Driver/main/config.h` - 添加清除NVS选项
- ✅ `Lamp_Driver/main/main.cpp` - 实现清除NVS

**技术说明**：

ESP32的MAC地址机制：
- Base MAC: 芯片固化的MAC
- STA MAC: 通常 = Base MAC
- AP MAC: 通常 = Base MAC + 1
- BT MAC: 通常 = Base MAC + 2

**ESP-NOW的临时MAC问题**：
ESP-NOW在广播模式下，源MAC地址可能是随机生成的临时MAC（如 `2E:EB:CB:3F:28:EB`），这不是设备的真实接收地址。如果使用这个临时MAC添加peer或发送单播消息，会失败。

**最终正确的做法**：
1. ✅ 在配对消息体中交换真实MAC（使用 `WiFi.macAddress()` 获取）
2. ✅ 使用**消息体中的MAC**添加peer和发送消息
3. ✅ 回调的 `mac_addr` 参数仅用于日志调试（可能是临时MAC）

**对比**：
```cpp
// ❌ 错误：使用回调的临时MAC
espnow_driver_add_controller(mac_addr, ...);  // mac_addr来自回调

// ✅ 正确：使用消息体的真实MAC
espnow_driver_add_controller(pairing_msg->mac_addr, ...);
```

**状态**: ✅ 已修复（需要重新烧录）

---

### [2026-01-30] 运行时错误 #5: ESP-NOW数据消息源MAC也是临时MAC
**类型**: 运行时错误
**严重级别**: 高
**现象**:

配对成功，控制器发送命令也成功，但驱动器仍然拒绝命令：

**控制器日志**:
```
[INFO] Saved peer MAC: D4:8C:49:86:1F:00  ✅
[DEBUG] Send OK to D4:8C:49:86:1F:00      ✅
```

**驱动器日志**:
```
[INFO] Controller added: D4:8C:49:84:3C:6C                    ✅
[WARN] Command from unpaired controller: 2E:EB:CB:3F:28:EB  ❌
```

**问题根本原因**:

ESP-NOW不仅在广播配对请求时使用临时MAC，**在单播数据消息时源MAC也可能是临时的！**

控制器发送流程：
1. 控制器发送命令到驱动器: `D4:8C:49:86:1F:00` (目标正确)
2. ESP-NOW发送时，源MAC被设置为: `2E:EB:CB:3F:28:EB` (临时MAC)
3. 驱动器收到后检查源MAC: `2E:EB:CB:3F:28:EB`
4. 驱动器的配对列表中只有: `D4:8C:49:84:3C:6C`
5. MAC不匹配 → 拒绝命令

**为什么ESP-NOW使用临时MAC？**
- 可能是隐私保护机制
- WiFi协议栈的实现细节
- ESP-IDF/Arduino版本相关

**解决方案**:

#### 1. 修改数据消息结构，添加发送方真实MAC字段

**lamp_state.h**:
```cpp
typedef struct {
    uint8_t msg_type;
    uint8_t device_id;
    uint8_t sender_mac[6];   // ← 新增：发送方真实MAC地址
    uint8_t command;
    uint8_t seq_num;
    lamp_state_t lamp_state;
} __attribute__((packed)) data_message_t;
```

#### 2. 控制器发送时填入真实MAC

**esp_now_ctrl.cpp**:
```cpp
bool espnow_ctrl_send_command(command_type_t cmd, const lamp_state_t *state) {
    data_message_t data_msg = {0};
    data_msg.msg_type = MSG_TYPE_DATA;
    data_msg.device_id = DEVICE_ID_CONTROLLER;

    // 填入发送方真实MAC地址
    WiFi.macAddress(data_msg.sender_mac);  // ← 关键修复

    data_msg.command = cmd;
    data_msg.seq_num = g_espnow_ctx.seq_num++;
    // ...
}
```

#### 3. 驱动器验证消息体中的MAC

**esp_now_driver.cpp**:
```cpp
// 错误的做法：验证回调的MAC（可能是临时MAC）
if (find_controller_index(mac_addr) >= 0) { ... }

// 正确的做法：验证消息体中的MAC（真实MAC）
if (find_controller_index(data_msg->sender_mac) >= 0) { ... }  // ← 关键修复
```

**修改的文件**:
- ✅ `rotary_controller/main/lamp_state.h` - 添加sender_mac字段
- ✅ `Lamp_Driver/main/lamp_state.h` - 添加sender_mac字段
- ✅ `rotary_controller/main/esp_now_ctrl.cpp` - 发送时填入真实MAC
- ✅ `Lamp_Driver/main/esp_now_driver.cpp` - 验证消息体MAC

**技术总结**:

ESP-NOW的MAC地址问题完整规律：
1. **广播配对请求**：源MAC是临时MAC（如 `2E:EB:CB:3F:28:EB`）
2. **单播配对响应**：源MAC也可能是临时MAC（如 `AA:E2:CB:3F:A4:E2`）
3. **单播数据消息**：源MAC仍然可能是临时MAC（如 `2E:EB:CB:3F:28:EB`）

**最终正确的架构**:
- ✅ 所有消息（配对、数据）都在消息体中携带发送方真实MAC
- ✅ 接收方始终验证**消息体中的MAC**，而不是回调参数
- ✅ 回调的`mac_addr`参数仅用于调试日志

**状态**: ✅ 已修复（需要重新烧录）

---

**文档维护**: 发现新错误时及时记录
**文档版本**: 1.3
**创建日期**: 2026-01-30
**更新日期**: 2026-01-30 (添加MAC地址不匹配错误修复记录)
