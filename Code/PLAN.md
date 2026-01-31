# 智能台灯 ESP-NOW 控制系统重构开发计划

## 项目概述
- **控制器**: ESP8684-MINI-1U 模块 + 旋转编码器，电池供电
- **驱动器**: ESP8684-MINI-1U 模块 + LED驱动（双通道色温调节）
- **通信协议**: ESP-NOW
- **开发框架**: Arduino as ESP-IDF Component

## 当前代码分析

### 控制器 (rotary_controller) 存在的问题
1. **代码冗余严重**
   - setup()和loop()中存在大量注释代码（约40%的代码被注释）
   - 硬编码的MAC地址配对方式
   - 配对逻辑(autoPairing)实现但未启用

2. **架构问题**
   - 全局变量过多且命名不规范
   - 状态机逻辑混乱（PairingStatus和LedState未有效联动）
   - 缺少模块化设计

3. **功能缺陷**
   - 深度睡眠功能被禁用
   - 没有实现快速唤醒机制
   - NVS存储逻辑不完整
   - 编码器控制逻辑不完整（缺少开灯时的旋转激活）

### 驱动器 (Lamp_Driver) 存在的问题
1. **代码冗余**
   - 测试代码未清理（format_test硬编码为true）
   - 硬编码的客户端MAC地址
   - 注释掉的配对响应代码

2. **LED控制逻辑缺陷**
   - led_power_on()从0淡入到current_duty（当灯关闭时current_duty为0，无法正常开灯）
   - 没有保存上次的亮度和色温状态
   - LEDC淡入淡出参数需要优化

3. **ESP-NOW配对**
   - 被动配对逻辑不完整
   - 没有存储配对信息到NVS

## 技术调研结果

### Arduino ESP32 v3.3.6 (2026-01-21)
- 支持ESP-NOW V2协议
- ESP32-C2需要作为ESP-IDF组件使用
- 参考: [Arduino ESP32 ESP-NOW文档](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/espnow.html)

### AiEsp32RotaryEncoder v1.3
- 支持加速、边界设置、中断驱动
- 新增isButtonPulldown选项（ESP32推荐设置为true）
- 参考: [AiEsp32RotaryEncoder GitHub](https://github.com/igorantolic/ai-esp32-rotary-encoder)

### ESP32-C2 深度睡眠特性
- 支持GPIO唤醒（`esp_deep_sleep_enable_gpio_wakeup`）
- RTC存储器可在深度睡眠期间保持数据
- 建议使用WiFi持久化模式减少重连时间

## 重构策略

### 设计原则
1. **模块化**: 按功能划分模块（ESP-NOW通信、LED控制、编码器处理、电源管理）
2. **状态机驱动**: 清晰的状态转换逻辑
3. **面向对象思想**: 使用C结构体+函数指针模拟类
4. **低功耗优化**: 最小化唤醒时间，快速完成任务后休眠

### 命名规范
- **模块前缀**: `espnow_`, `led_`, `encoder_`, `power_`
- **全局配置**: `CONFIG_` 前缀
- **状态枚举**: 大写蛇形命名（如 `PAIRING_STATE_IDLE`）
- **函数**: 小写蛇形命名（如 `espnow_init_controller`）
- **结构体**: `_t` 后缀（如 `lamp_state_t`）

## 详细开发计划

### 阶段 1: 代码清理与重构 (基础架构)

#### 1.1 控制器重构
**文件结构**:
```
rotary_controller/
├── main/
│   ├── main.cpp           # 主程序入口
│   ├── config.h           # 配置定义
│   ├── esp_now_ctrl.cpp   # ESP-NOW通信模块
│   ├── esp_now_ctrl.h
│   ├── encoder_handler.cpp# 编码器处理模块
│   ├── encoder_handler.h
│   ├── power_mgmt.cpp     # 电源管理模块
│   ├── power_mgmt.h
│   └── lamp_state.h       # 状态定义
```

**重构内容**:
- 移除所有注释代码
- 提取配置到config.h（GPIO定义、PWM参数、超时设置等）
- 创建lamp_state_t统一管理灯光状态（亮度、色温、开关状态）
- 封装ESP-NOW初始化、配对、发送逻辑
- 封装编码器初始化、中断处理、按键处理逻辑
- 封装深度睡眠配置、NVS存储/读取逻辑

#### 1.2 驱动器重构
**文件结构**:
```
Lamp_Driver/
├── main/
│   ├── main.cpp           # 主程序入口
│   ├── config.h           # 配置定义
│   ├── esp_now_driver.cpp # ESP-NOW通信模块
│   ├── esp_now_driver.h
│   ├── led_controller.cpp # LED控制模块
│   ├── led_controller.h
│   └── lamp_state.h       # 状态定义（与控制器共享）
```

**重构内容**:
- 移除测试代码和硬编码MAC地址
- 创建LED控制器模块，封装LEDC初始化、淡入淡出、状态管理
- 修复led_power_on逻辑（保存上次状态到NVS）
- 封装ESP-NOW配对响应和数据接收处理
- 统一错误处理和日志输出

### 阶段 2: ESP-NOW 自动配对功能实现

#### 2.1 配对协议设计
```cpp
// 配对状态机
enum pairing_state_t {
    PAIRING_STATE_INIT,       // 初始化
    PAIRING_STATE_SCANNING,   // 控制器扫描
    PAIRING_STATE_REQUESTING, // 发送配对请求
    PAIRING_STATE_WAITING,    // 等待响应
    PAIRING_STATE_PAIRED,     // 已配对
    PAIRING_STATE_TIMEOUT     // 超时
};

// 配对消息增强
struct pairing_message_t {
    uint8_t msg_type;         // PAIRING
    uint8_t device_role;      // 0=驱动器, 1=控制器
    uint8_t mac_addr[6];      // 设备MAC
    uint8_t channel;          // WiFi信道
    uint32_t timestamp;       // 时间戳（防重放）
    uint16_t device_id;       // 设备ID
};
```

#### 2.2 控制器配对流程
1. **启动时检查NVS**
   - 读取上次配对的驱动器MAC和信道
   - 尝试在该信道上发送探测包
   - 如果3次探测失败，进入扫描模式

2. **扫描模式**
   - 遍历信道1-13（ESP32-C2支持）
   - 每个信道广播配对请求（FF:FF:FF:FF:FF:FF）
   - 超时时间：每信道500ms

3. **配对确认**
   - 收到驱动器响应后，添加为peer
   - 保存MAC和信道到NVS
   - 发送确认消息

#### 2.3 驱动器配对流程
1. **被动监听**
   - 启动时初始化ESP-NOW，监听配对请求
   - 收到配对请求时，发送响应并添加控制器为peer

2. **配对响应**
   - 发送包含自身MAC和信道的响应
   - 保存控制器MAC到NVS

3. **多控制器支持预留**
   - 设计上支持最多3个控制器配对

### 阶段 3: 深度睡眠与快速唤醒优化

#### 3.1 问题根因分析
当前唤醒慢的原因：
1. WiFi重新初始化耗时（~200-500ms）
2. ESP-NOW重新配对耗时（~1-3秒）
3. 没有使用RTC内存保存状态

#### 3.2 优化方案

**方案A: RTC内存缓存状态（推荐）**
```cpp
// 使用RTC_DATA_ATTR保存关键状态
RTC_DATA_ATTR static bool is_paired = false;
RTC_DATA_ATTR static uint8_t peer_mac[6];
RTC_DATA_ATTR static uint8_t peer_channel;
RTC_DATA_ATTR static lamp_state_t last_lamp_state;
```

**方案B: WiFi/ESP-NOW持久化**
- 使用`esp_wifi_set_storage(WIFI_STORAGE_RAM)`减少Flash读写
- 配对成功后，不deinit ESP-NOW，仅deep sleep

**唤醒流程优化**:
1. 检测唤醒源（编码器旋转/按键）
2. 从RTC内存恢复配对状态
3. 快速初始化WiFi和ESP-NOW（跳过扫描）
4. 直接在已知信道上发送命令
5. 目标：唤醒到发送命令 < 200ms

#### 3.3 睡眠策略
- **活动后延迟**: 最后一次操作后5秒进入睡眠
- **唤醒源配置**:
  - GPIO2/3（编码器A/B）下降沿唤醒
  - GPIO4（编码器按键）下降沿唤醒
- **睡眠前准备**:
  - 保存当前灯光状态到RTC内存
  - 保存配对信息到NVS（仅变化时）

### 阶段 4: 编码器控制逻辑完善

#### 4.1 需求细化
| 操作 | 灯光状态 | 行为 |
|------|---------|------|
| 单击按键 | 关灯 | 开灯（恢复上次亮度和色温）|
| 旋转编码器 | 关灯 | 开灯（恢复上次亮度和色温）|
| 顺时针旋转 | 开灯 | 增加亮度（色温不变）|
| 逆时针旋转 | 开灯 | 降低亮度（色温不变）|
| 按住+顺时针 | 开灯 | 增加色温（冷白，亮度不变）|
| 按住+逆时针 | 开灯 | 降低色温（暖白，亮度不变）|
| 长按2秒 | 开灯 | 关灯 |

#### 4.2 状态机设计
```cpp
enum lamp_mode_t {
    LAMP_MODE_OFF,           // 关灯
    LAMP_MODE_BRIGHTNESS,    // 亮度调节模式
    LAMP_MODE_TEMPERATURE    // 色温调节模式
};

// 编码器事件
enum encoder_event_t {
    ENCODER_EVENT_ROTATE_CW,    // 顺时针
    ENCODER_EVENT_ROTATE_CCW,   // 逆时针
    ENCODER_EVENT_BTN_PRESS,    // 按下
    ENCODER_EVENT_BTN_RELEASE,  // 释放
    ENCODER_EVENT_SHORT_CLICK,  // 短按
    ENCODER_EVENT_LONG_PRESS    // 长按
};
```

#### 4.3 淡入淡出实现
**控制器端**:
- 计算目标亮度/色温
- 发送目标duty值给驱动器
- 驱动器端负责淡入淡出

**驱动器端**:
- 使用`ledcFade()`实现平滑过渡
- 淡入淡出时间：
  - 开关灯：2000ms
  - 调节：每级3ms（与编码器速度同步）

### 阶段 5: 数据结构和通信协议优化

#### 5.1 统一lamp_state结构
```cpp
typedef struct {
    bool is_on;              // 开关状态
    uint16_t brightness;     // 亮度 (0-511)
    float temperature;       // 色温比例 (0.0-1.0, 0=2700K, 1=5000K)
    uint16_t duty_ch0;       // 冷白通道PWM
    uint16_t duty_ch1;       // 暖白通道PWM
} lamp_state_t;
```

#### 5.2 通信消息优化
```cpp
// 命令类型
enum command_type_t {
    CMD_SET_LAMP_STATE,      // 设置灯光状态
    CMD_LAMP_STATE_RESPONSE, // 状态响应
    CMD_PAIR_REQUEST,        // 配对请求
    CMD_PAIR_RESPONSE        // 配对响应
};

// 数据消息
typedef struct {
    uint8_t msg_type;        // DATA
    uint8_t cmd;             // command_type_t
    uint8_t seq_num;         // 序列号（检测丢包）
    lamp_state_t lamp_state; // 灯光状态
} data_message_t;
```

### 阶段 6: CMake配置检查与优化

#### 6.1 检查项
- [ ] 确认Arduino组件正确链接
- [ ] 确认AiEsp32RotaryEncoder和Button2组件路径正确
- [ ] 添加编译优化标志（-O2 for speed）
- [ ] 确认ESP32-C2目标正确配置

#### 6.2 sdkconfig关键配置
```
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8
CONFIG_ESP32C2_LIGHTSLEEP_GPIO_RESET_WORKAROUND=y
CONFIG_FREERTOS_HZ=1000
```

### 阶段 7: 测试与验证

#### 7.1 单元测试
- ESP-NOW配对测试（成功率、延迟）
- 深度睡眠唤醒测试（唤醒时间、状态恢复）
- 编码器响应测试（所有操作组合）
- LED淡入淡出测试（平滑度、准确性）

#### 7.2 集成测试
- 冷启动配对场景
- 唤醒-控制-睡眠循环
- 断电恢复测试（NVS数据完整性）
- 长时间运行稳定性

## 开发时间线

| 阶段 | 预计工作量 | 输出物 |
|------|----------|--------|
| 阶段1: 代码重构 | 核心工作 | 重构后的代码文件 |
| 阶段2: 自动配对 | 中等工作量 | 配对功能模块 |
| 阶段3: 深度睡眠优化 | 重点优化 | 电源管理模块 |
| 阶段4: 控制逻辑 | 中等工作量 | 编码器处理模块 |
| 阶段5: 协议优化 | 轻量工作 | 协议定义头文件 |
| 阶段6: CMake检查 | 轻量工作 | CMakeLists.txt |
| 阶段7: 测试验证 | 迭代进行 | 测试报告 |

## 风险与挑战

### 技术风险
1. **ESP32-C2深度睡眠唤醒时间优化难度**
   - 缓解：使用RTC内存+WiFi持久化双保险

2. **ESP-NOW可靠性**
   - 缓解：添加重传机制和序列号检测

3. **编码器中断与睡眠兼容性**
   - 缓解：使用GPIO唤醒而非中断唤醒

### 开发约束
- 无法实时编译测试，需要一次性设计完善
- 需要详细的日志输出辅助调试

## 成功标准

1. **代码质量**
   - [ ] 无冗余代码和注释
   - [ ] 模块化清晰，单一职责
   - [ ] 变量命名规范统一

2. **功能完整性**
   - [ ] 自动配对成功率>95%
   - [ ] 控制响应<200ms
   - [ ] 支持所有需求的编码器操作

3. **性能指标**
   - [ ] 深度睡眠唤醒到发送命令<200ms
   - [ ] 空闲5秒后进入睡眠
   - [ ] LED淡入淡出平滑无闪烁

## 参考资料

- [Arduino ESP32 Documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [ESP-NOW API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/espnow.html)
- [AiEsp32RotaryEncoder GitHub](https://github.com/igorantolic/ai-esp32-rotary-encoder)
- [ESP32-C2 Technical Reference](https://documentation.espressif.com/esp8684_technical_reference_manual_en.pdf)

---

**文档版本**: 1.0
**创建日期**: 2026-01-30
**作者**: Claude (Embedded Programming Expert)
