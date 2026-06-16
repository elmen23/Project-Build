#include "PWMManager.h"

PWMManager::PWMManager(uint8_t pinA, uint8_t pinB)
    : _pinA(pinA), _pinB(pinB) {}

PWMManager::~PWMManager() {
    stop();
}

bool PWMManager::begin(const CoreParams& params) {
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, _pinA);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, _pinB);

    mcpwm_config_t cfg = {};
    cfg.frequency    = (uint32_t)params.freq;
    cfg.cmpr_a       = 0.0f;
    cfg.cmpr_b       = 0.0f;
    cfg.counter_mode = MCPWM_UP_COUNTER;
    cfg.duty_mode    = MCPWM_DUTY_MODE_0;

    esp_err_t err = mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
    if (err != ESP_OK) {
        Serial.printf("[PWM] MCPWM init failed: %d\n", (int)err);
        return false;
    }

    setDeadTime(params.deadTimeNs);

    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);

    _lastDuty = 0.0f;
    Serial.printf("[PWM] init ok — freq=%u Hz, dt=%.0f ns\n",
                  cfg.frequency, params.deadTimeNs);
    return true;
}

void PWMManager::start() {
    _running = true;
    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
    // Re-apply duty after start (timer reset clears compare registers)
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, _lastDuty);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, _lastDuty);
}

void PWMManager::stop() {
    if (!_running) return;
    _running = false;
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);
    mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
}

void PWMManager::setDuty(float pct) {
    _lastDuty = constrain(pct, 0.0f, 100.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, _lastDuty);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, _lastDuty);
}

void PWMManager::setFrequency(float hz) {
    hz = constrain(hz, 1000.0f, 400000.0f);
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, (uint32_t)hz);
    // Re-apply duty after frequency change (timer period changed)
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, _lastDuty);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, _lastDuty);
}

void PWMManager::setDeadTime(float ns) {
    uint32_t ticks = nsToTicks(ns);
    if (ticks > 0) {
        mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
                              MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, ticks, ticks);
        Serial.printf("[PWM] dead-time %.0f ns → %u ticks\n", ns, ticks);
    } else {
        mcpwm_deadtime_disable(MCPWM_UNIT_0, MCPWM_TIMER_0);
        Serial.println("[PWM] dead-time disabled");
    }
}

// ── Private ──────────────────────────────────────────────────────

uint32_t PWMManager::nsToTicks(float ns) {
    if (ns <= 0.0f) return 0;
    return (uint32_t)((ns / DT_CLOCK_NS) + 0.5f);
}
