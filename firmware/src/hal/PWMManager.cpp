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
        Serial.printf("[PWM] init fail: %d\n", (int)err);
        return false;
    }

    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);

    setDeadTime(params.deadTimeNs);

    _lastDuty = 0.0f;
    _state = RunState::IDLE;
    Serial.printf("[PWM] init ok — freq=%u Hz\n", cfg.frequency);
    return true;
}

void PWMManager::start(uint32_t softStartMs) {
    if (_running) return;

    _running = true;
    _softStarting = true;
    _state = RunState::SOFT_START;
    _ssStart = millis();
    _ssDuration = softStartMs;
    _ssTargetDuty = 50.0f;
    _lastDuty = 0.0f;

    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);

    Serial.printf("[PWM] start → %.1f%% over %ums\n", _ssTargetDuty, softStartMs);
}

void PWMManager::stop() {
    if (!_running) return;
    _running = false;
    _softStarting = false;
    _state = RunState::IDLE;
    _lastDuty = 0.0f;

    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);
    mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
    Serial.printf("[PWM] stop\n");
}

void PWMManager::softStartTick() {
    if (!_softStarting) return;

    uint32_t elapsed = millis() - _ssStart;
    if (elapsed >= _ssDuration) {
        _lastDuty = _ssTargetDuty;
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, _lastDuty);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, _lastDuty);
        _softStarting = false;
        _state = RunState::RUNNING;
        Serial.printf("[PWM] soft-start done — %.1f%%\n", _lastDuty);
        return;
    }

    float d = _ssTargetDuty * ((float)elapsed / _ssDuration);
    if (d != _lastDuty) {
        _lastDuty = d;
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, _lastDuty);
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, _lastDuty);
    }
}

void PWMManager::setFrequency(float hz) {
    hz = constrain(hz, CoreParams::FREQ_MIN, CoreParams::FREQ_MAX);
    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, (uint32_t)hz);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, _lastDuty);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, _lastDuty);
}

void PWMManager::setDeadTime(float ns) {
    uint32_t ticks = nsToTicks(ns);
    if (ticks > 0) {
        mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
                              MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, ticks, ticks);
    } else {
        mcpwm_deadtime_disable(MCPWM_UNIT_0, MCPWM_TIMER_0);
    }
}

uint32_t PWMManager::nsToTicks(float ns) {
    if (ns <= 0.0f) return 0;
    return (uint32_t)((ns / DT_CLOCK_NS) + 0.5f);
}
