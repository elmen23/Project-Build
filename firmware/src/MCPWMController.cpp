#include "MCPWMController.h"

/* ─── Clock: APB_CLK = 80 MHz, prescaler fixed to 1 (default) ───
 *  1 tick = 12.5 ns
 */

static constexpr float TICK_NS = 12.5f;

esp_err_t MCPWMController::begin(float freq_hz, float dead_time_ns) {
    _freq = constrain(freq_hz, (float)FREQ_MIN, (float)FREQ_MAX);
    _dtNs = constrain(dead_time_ns, DT_MIN, DT_MAX);

    /* Bind GPIOs to MCPWM unit 0 */
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_PWM_HIGH);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_PWM_LOW);

    /* Timer config — duty 50% fixed */
    mcpwm_config_t cfg = {};
    cfg.frequency    = (uint32_t)_freq;
    cfg.cmpr_a       = 50.0f;          /* 50 % */
    cfg.cmpr_b       = 0.0f;
    cfg.counter_mode = MCPWM_UP_COUNTER;
    cfg.duty_mode    = MCPWM_DUTY_MODE_0;

    esp_err_t err = mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
    if (err != ESP_OK) return err;

    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0,
                         MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 50.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 50.0f);

    /* Complementary PWM with dead time */
    uint32_t ticks = nsToTicks(_dtNs);
    if (ticks > 0) {
        mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
                              MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE,
                              ticks, ticks);
    }

    /* Start the timer but output stays at 0 until we call start() */
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);

    _running = false;
    return ESP_OK;
}

void MCPWMController::start() {
    if (_running) return;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 50.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 50.0f);
    _running = true;
}

void MCPWMController::stop() {
    if (!_running) return;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);
    _running = false;
}

void MCPWMController::setFrequency(float hz) {
    _freq = constrain(hz, (float)FREQ_MIN, (float)FREQ_MAX);
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, (uint32_t)_freq);
    if (_running) {
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 50.0f);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 50.0f);
    }
}

void MCPWMController::setDeadTime(float ns) {
    _dtNs = constrain(ns, DT_MIN, DT_MAX);
    uint32_t ticks = nsToTicks(_dtNs);

    mcpwm_deadtime_disable(MCPWM_UNIT_0, MCPWM_TIMER_0);
    if (ticks > 0) {
        mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
                              MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE,
                              ticks, ticks);
    }
}

uint32_t MCPWMController::nsToTicks(float ns) const {
    if (ns <= DT_MIN + 0.5f) return 0;
    return (uint32_t)(ns / TICK_NS + 0.5f);
}
