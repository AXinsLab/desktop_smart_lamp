/**
 * @file led_controller.cpp
 * @brief LED控制模块实现
 */

#include "led_controller.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

// ==================== 模块私有变量 ====================
static lamp_state_t g_current_state = {0};    // 当前实际状态
static lamp_state_t g_last_state = {0};       // 上次开灯状态（用于恢复）
static bool g_is_initialized = false;

// ==================== 公开函数实现 ====================

/**
 * @brief 初始化LED控制器
 */
bool led_ctrl_init(void) {
    LOG_I("Initializing LED controller...");

    // 设置LEDC时钟源为XTAL（更稳定）
    if (!ledcSetClockSource(LEDC_USE_XTAL_CLK)) {
        LOG_E("Failed to set LEDC clock source");
        return false;
    }

    // 配置两个PWM通道
    if (!ledcAttach(CONFIG_LED_CH0_PIN, CONFIG_PWM_FREQUENCY, CONFIG_PWM_RESOLUTION)) {
        LOG_E("Failed to attach LED CH0");
        return false;
    }

    if (!ledcAttach(CONFIG_LED_CH1_PIN, CONFIG_PWM_FREQUENCY, CONFIG_PWM_RESOLUTION)) {
        LOG_E("Failed to attach LED CH1");
        ledcDetach(CONFIG_LED_CH0_PIN);
        return false;
    }

    // 初始化为关灯状态
    ledcWrite(CONFIG_LED_CH0_PIN, 0);
    ledcWrite(CONFIG_LED_CH1_PIN, 0);

    // 初始化当前状态
    g_current_state.is_on = false;
    g_current_state.brightness = 255;  // 默认中等亮度
    g_current_state.temperature = 0.5f; // 默认中性色温
    g_current_state.duty_ch0 = 0;
    g_current_state.duty_ch1 = 0;

    // 初始化上次状态为默认值
    g_last_state.is_on = false;
    g_last_state.brightness = 255;
    g_last_state.temperature = 0.5f;
    lamp_calculate_duty(&g_last_state);

    g_is_initialized = true;

    LOG_I("LED controller initialized: CH0=GPIO%d, CH1=GPIO%d, Freq=%dHz, Res=%dbit",
          CONFIG_LED_CH0_PIN, CONFIG_LED_CH1_PIN,
          CONFIG_PWM_FREQUENCY, CONFIG_PWM_RESOLUTION);

    return true;
}

/**
 * @brief 设置灯光状态
 */
bool led_ctrl_set_state(const lamp_state_t *target_state) {
    if (!g_is_initialized || target_state == NULL) {
        return false;
    }

    // 计算淡入淡出时间（根据duty变化量）
    uint32_t fade_time_ch0 = abs((int)target_state->duty_ch0 - (int)g_current_state.duty_ch0) * CONFIG_LED_STEP_FADE_TIME_MS;
    uint32_t fade_time_ch1 = abs((int)target_state->duty_ch1 - (int)g_current_state.duty_ch1) * CONFIG_LED_STEP_FADE_TIME_MS;

    LOG_D("LED fade: CH0 %d->%d (%lums), CH1 %d->%d (%lums)",
          g_current_state.duty_ch0, target_state->duty_ch0, fade_time_ch0,
          g_current_state.duty_ch1, target_state->duty_ch1, fade_time_ch1);

    // 执行淡入淡出
    ledcFade(CONFIG_LED_CH0_PIN,
             g_current_state.duty_ch0,
             target_state->duty_ch0,
             fade_time_ch0);

    ledcFade(CONFIG_LED_CH1_PIN,
             g_current_state.duty_ch1,
             target_state->duty_ch1,
             fade_time_ch1);

    // 更新当前状态
    memcpy(&g_current_state, target_state, sizeof(lamp_state_t));

    LOG_I("LED state set: on=%d, brightness=%u, temp=%.2f",
          g_current_state.is_on,
          g_current_state.brightness,
          g_current_state.temperature);

    return true;
}

/**
 * @brief 开灯
 */
bool led_ctrl_power_on(const lamp_state_t *target_state) {
    if (!g_is_initialized) {
        return false;
    }

    lamp_state_t on_state;

    if (target_state != NULL) {
        // 使用指定的目标状态
        memcpy(&on_state, target_state, sizeof(lamp_state_t));
    } else {
        // 使用上次保存的状态
        memcpy(&on_state, &g_last_state, sizeof(lamp_state_t));
    }

    on_state.is_on = true;
    lamp_calculate_duty(&on_state);

    LOG_I("Power ON: brightness=%u, temp=%.2f", on_state.brightness, on_state.temperature);

    // 从0淡入到目标亮度
    ledcFade(CONFIG_LED_CH0_PIN, 0, on_state.duty_ch0, CONFIG_LED_POWER_FADE_TIME_MS);
    ledcFade(CONFIG_LED_CH1_PIN, 0, on_state.duty_ch1, CONFIG_LED_POWER_FADE_TIME_MS);

    // 更新当前状态
    memcpy(&g_current_state, &on_state, sizeof(lamp_state_t));

    return true;
}

/**
 * @brief 关灯
 */
bool led_ctrl_power_off(void) {
    if (!g_is_initialized) {
        return false;
    }

    LOG_I("Power OFF");

    // 保存当前状态（如果灯是开着的）
    if (g_current_state.is_on) {
        led_ctrl_save_last_state();
    }

    // 从当前亮度淡出到0
    ledcFade(CONFIG_LED_CH0_PIN, g_current_state.duty_ch0, 0, CONFIG_LED_POWER_FADE_TIME_MS);
    ledcFade(CONFIG_LED_CH1_PIN, g_current_state.duty_ch1, 0, CONFIG_LED_POWER_FADE_TIME_MS);

    // 更新当前状态
    g_current_state.is_on = false;
    g_current_state.duty_ch0 = 0;
    g_current_state.duty_ch1 = 0;

    return true;
}

/**
 * @brief 获取当前状态
 */
bool led_ctrl_get_current_state(lamp_state_t *state_out) {
    if (!g_is_initialized || state_out == NULL) {
        return false;
    }

    memcpy(state_out, &g_current_state, sizeof(lamp_state_t));
    return true;
}

/**
 * @brief 保存上次状态
 */
void led_ctrl_save_last_state(void) {
    if (!g_is_initialized) {
        return;
    }

    // 仅保存亮度和色温，不保存开关状态
    g_last_state.brightness = g_current_state.brightness;
    g_last_state.temperature = g_current_state.temperature;
    g_last_state.is_on = false;
    lamp_calculate_duty(&g_last_state);

    LOG_D("Last state saved: brightness=%u, temp=%.2f",
          g_last_state.brightness, g_last_state.temperature);
}

/**
 * @brief 恢复上次状态
 */
bool led_ctrl_restore_last_state(lamp_state_t *state_out) {
    if (!g_is_initialized || state_out == NULL) {
        return false;
    }

    memcpy(state_out, &g_last_state, sizeof(lamp_state_t));

    LOG_D("Last state restored: brightness=%u, temp=%.2f",
          state_out->brightness, state_out->temperature);

    return true;
}
