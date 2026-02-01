/**
 * @file encoder_handler.cpp
 * @brief 编码器和按键处理模块实现
 */

#include "encoder_handler.h"
#include "config.h"
#include <Arduino.h>
#include <AiEsp32RotaryEncoder.h>
#include <Button2.h>

// ==================== 模块私有变量 ====================
static AiEsp32RotaryEncoder *g_rotary_encoder = NULL;
static Button2 *g_encoder_button = NULL;
static encoder_mode_t g_current_mode = ENCODER_MODE_BRIGHTNESS;
static int32_t g_last_encoder_value = 0;
static uint32_t g_last_activity_time = 0;
static bool g_activity_flag = false;

// ==================== 编码器中断处理 ====================
void IRAM_ATTR encoder_isr(void) {
    if (g_rotary_encoder != NULL) {
        g_rotary_encoder->readEncoder_ISR();
    }
}

// ==================== 按键回调函数 ====================

// 按键事件标志
static encoder_event_t g_button_event = ENCODER_EVENT_NONE;
static uint32_t g_button_press_start_time = 0;
static bool g_button_was_pressed = false;

// 按键按住期间是否旋转过编码器（用于避免调节色温时误触发长按/重置）
static bool g_encoder_rotated_during_press = false;

/**
 * @brief 短按回调（用于Button2）
 */
static void on_button_click(Button2 &btn) {
    LOG_I("Button clicked");
    g_button_event = ENCODER_EVENT_BTN_CLICK;
    g_last_activity_time = millis();
    g_activity_flag = true;
}

/**
 * @brief 长按回调（用于Button2）
 */
static void on_button_long_press(Button2 &btn) {
    // 关键逻辑：如果按住期间有旋转编码器，则忽略长按事件（用户在调节色温）
    if (g_encoder_rotated_during_press) {
        LOG_D("Long press ignored (encoder was rotated during press)");
        return;
    }

    LOG_I("Button long pressed");
    g_button_event = ENCODER_EVENT_BTN_LONG_PRESS;
    g_last_activity_time = millis();
    g_activity_flag = true;
}

// ==================== 公开函数实现 ====================

/**
 * @brief 初始化编码器和按键
 */
bool encoder_init(void) {
    LOG_I("Initializing encoder and button...");

    // 创建编码器对象
    g_rotary_encoder = new AiEsp32RotaryEncoder(
        CONFIG_ENCODER_PIN_A,
        CONFIG_ENCODER_PIN_B,
        -1,  // 按键引脚由Button2处理
        CONFIG_ENCODER_STEPS
    );

    if (g_rotary_encoder == NULL) {
        LOG_E("Failed to create encoder object");
        return false;
    }

    // 配置编码器
    g_rotary_encoder->areEncoderPinsPulldownforEsp32 = false;  // 使用内部上拉
    g_rotary_encoder->begin();
    g_rotary_encoder->setup(encoder_isr);
    g_rotary_encoder->setBoundaries(CONFIG_ENCODER_MIN_VALUE, CONFIG_ENCODER_MAX_VALUE, false);
    g_rotary_encoder->setAcceleration(CONFIG_ENCODER_ACCELERATION);

    g_last_encoder_value = g_rotary_encoder->readEncoder();

    // 创建按键对象
    g_encoder_button = new Button2(CONFIG_ENCODER_PIN_BTN);
    if (g_encoder_button == NULL) {
        LOG_E("Failed to create button object");
        delete g_rotary_encoder;
        g_rotary_encoder = NULL;
        return false;
    }

    // 配置按键
    g_encoder_button->begin(CONFIG_ENCODER_PIN_BTN, INPUT_PULLUP, true);
    g_encoder_button->setDebounceTime(CONFIG_BTN_DEBOUNCE_MS);
    g_encoder_button->setLongClickTime(CONFIG_BTN_LONG_PRESS_MS);
    g_encoder_button->setClickHandler(on_button_click);
    g_encoder_button->setLongClickDetectedHandler(on_button_long_press);

    g_last_activity_time = millis();
    g_activity_flag = false;

    LOG_I("Encoder and button initialized");
    return true;
}

/**
 * @brief 处理编码器和按键事件
 */
encoder_event_t encoder_process(const lamp_state_t *current_state, lamp_state_t *new_state) {
    if (g_rotary_encoder == NULL || g_encoder_button == NULL ||
        current_state == NULL || new_state == NULL) {
        return ENCODER_EVENT_NONE;
    }

    encoder_event_t event = ENCODER_EVENT_NONE;

    // ===== 关键修复：先检测编码器旋转，再调用按键loop() =====
    // 这样可以在Button2的长按回调触发前设置好标志位，避免竞态条件

    // 检查编码器旋转（优先检测）
    if (g_rotary_encoder->encoderChanged()) {
        int32_t current_value = g_rotary_encoder->readEncoder();
        int32_t delta = current_value - g_last_encoder_value;
        g_last_encoder_value = current_value;

        LOG_D("Encoder changed: delta=%ld", delta);

        g_last_activity_time = millis();
        g_activity_flag = true;

        // 关键逻辑：如果按键正在按下，立即标记为"色温调节模式"
        // 一旦进入此模式，本次按键期间将不再触发长按/重置
        if (g_encoder_button->isPressed()) {
            if (!g_encoder_rotated_during_press) {
                LOG_D("Entered color temp adjustment mode");
            }
            g_encoder_rotated_during_press = true;
        }

        // 复制当前状态到新状态
        memcpy(new_state, current_state, sizeof(lamp_state_t));

        // 判断旋转方向
        if (delta > 0) {
            event = ENCODER_EVENT_ROTATE_CW;
        } else if (delta < 0) {
            event = ENCODER_EVENT_ROTATE_CCW;
        }

        // 如果灯是关闭的，任何旋转都先开灯
        if (!current_state->is_on) {
            LOG_I("Lamp off, turning on by rotation");
            new_state->is_on = true;
            lamp_calculate_duty(new_state);
            return event;
        }

        // 根据模式调节亮度或色温
        if (g_current_mode == ENCODER_MODE_BRIGHTNESS) {
            // 调节亮度
            int32_t new_brightness = (int32_t)current_state->brightness + (delta * CONFIG_BRIGHTNESS_STEP);

            // 限制范围
            if (new_brightness < CONFIG_MIN_BRIGHTNESS) {
                new_brightness = CONFIG_MIN_BRIGHTNESS;
            }
            if (new_brightness > CONFIG_MAX_BRIGHTNESS) {
                new_brightness = CONFIG_MAX_BRIGHTNESS;
            }

            new_state->brightness = (uint16_t)new_brightness;
            LOG_D("Brightness adjusted to %u", new_state->brightness);
        }
        else if (g_current_mode == ENCODER_MODE_TEMPERATURE) {
            // 调节色温
            float temp_delta = (float)delta * CONFIG_TEMPERATURE_STEP / 255.0f;
            float new_temp = current_state->temperature + temp_delta;

            // 限制范围 [0.0, 1.0]
            if (new_temp < 0.0f) {
                new_temp = 0.0f;
            }
            if (new_temp > 1.0f) {
                new_temp = 1.0f;
            }

            new_state->temperature = new_temp;
            LOG_D("Temperature adjusted to %.2f", new_state->temperature);
        }

        // 重新计算duty值
        lamp_calculate_duty(new_state);
    }

    // ===== 更新按键状态（在编码器检测之后，确保标志位已设置）=====
    g_encoder_button->loop();

    // 检测超长按（5秒重置）
    if (g_encoder_button->isPressed()) {
        if (!g_button_was_pressed) {
            // 按键刚按下：重置旋转标志
            g_button_press_start_time = millis();
            g_button_was_pressed = true;
            g_encoder_rotated_during_press = false;  // 每次按键重置标志
        } else {
            // 按键持续按下：检查是否达到5秒重置时长
            uint32_t press_duration = millis() - g_button_press_start_time;
            if (press_duration >= CONFIG_BTN_RESET_PRESS_MS) {
                // 关键逻辑：仅在未旋转编码器时触发重置
                if (!g_encoder_rotated_during_press) {
                    LOG_I("RESET: 5s press detected! (rotated_flag=%d)", g_encoder_rotated_during_press);
                    g_button_event = ENCODER_EVENT_BTN_RESET;
                    g_button_was_pressed = false;
                } else {
                    LOG_I("RESET ignored (encoder was rotated during press)");
                    g_button_was_pressed = false;
                }
            }
        }
    } else {
        g_button_was_pressed = false;
    }

    // 检查按键按下/释放（切换模式）
    if (g_encoder_button->isPressed()) {
        if (g_current_mode != ENCODER_MODE_TEMPERATURE) {
            g_current_mode = ENCODER_MODE_TEMPERATURE;
            LOG_D("Switched to TEMPERATURE mode");
        }
    } else {
        if (g_current_mode != ENCODER_MODE_BRIGHTNESS) {
            g_current_mode = ENCODER_MODE_BRIGHTNESS;
            LOG_D("Switched to BRIGHTNESS mode");
        }
    }

    // 检查按键事件（由Button2回调或超长按检测设置）
    if (g_button_event != ENCODER_EVENT_NONE) {
        if (g_button_event == ENCODER_EVENT_BTN_RESET) {
            // 超长按：重置配对（不修改灯光状态）
            event = ENCODER_EVENT_BTN_RESET;
        } else {
            memcpy(new_state, current_state, sizeof(lamp_state_t));

            if (g_button_event == ENCODER_EVENT_BTN_CLICK) {
                // 短按：仅在关灯状态下开灯，开灯状态下无操作
                if (!current_state->is_on) {
                    LOG_I("Short press: turning on lamp");
                    new_state->is_on = true;
                    lamp_calculate_duty(new_state);
                    event = ENCODER_EVENT_BTN_CLICK;
                } else {
                    LOG_D("Short press: lamp already on, no operation");
                    // 灯已开启，短按无操作
                }
            }
            else if (g_button_event == ENCODER_EVENT_BTN_LONG_PRESS) {
                // 长按：关灯
                LOG_I("Long press: turning off lamp");
                new_state->is_on = false;
                new_state->duty_ch0 = 0;
                new_state->duty_ch1 = 0;
                event = ENCODER_EVENT_BTN_LONG_PRESS;
            }
        }

        g_button_event = ENCODER_EVENT_NONE;  // 清除事件
    }

    return event;
}

/**
 * @brief 获取当前编码器模式
 */
encoder_mode_t encoder_get_mode(void) {
    return g_current_mode;
}

/**
 * @brief 重置编码器值
 */
void encoder_reset(void) {
    if (g_rotary_encoder != NULL) {
        g_rotary_encoder->setEncoderValue(0);
        g_last_encoder_value = 0;
        LOG_D("Encoder reset");
    }
}

/**
 * @brief 检查是否有活动
 */
bool encoder_has_activity(void) {
    return g_activity_flag;
}

/**
 * @brief 获取最后活动时间
 */
uint32_t encoder_get_last_activity_time(void) {
    return g_last_activity_time;
}
