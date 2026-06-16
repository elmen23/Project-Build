#pragma once
#include <Arduino.h>
#include "driver/mcpwm.h"
#include "core/Types.h"

#define PWM_PIN_A   GPIO_NUM_18
#define PWM_PIN_B   GPIO_NUM_19
#define DT_CLOCK_NS 12.5f

class PWMManager {
public:
    PWMManager(uint8_t pinA, uint8_t pinB);
    ~PWMManager();

    PWMManager(const PWMManager&) = delete;
    PWMManager& operator=(const PWMManager&) = delete;

    bool begin(const CoreParams& params);
    void start();
    void stop();
    bool isRunning() const { return _running; }

    void setDuty(float pct);
    void setFrequency(float hz);
    void setDeadTime(float ns);

private:
    const uint8_t _pinA, _pinB;
    volatile bool  _running = false;
    float          _lastDuty = 0.0f;

    static uint32_t nsToTicks(float ns);
};
