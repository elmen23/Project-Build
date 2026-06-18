#pragma once
#include <Arduino.h>
#include "core/Types.h"

class CTFeedback;
class PWMManager;

class PLLController {
public:
    enum State : uint8_t {
        IDLE = 0,
        SWEEPING,
        PLL_LOCK,
        PLL_UNLOCK
    };

    PLLController();
    ~PLLController() = default;

    PLLController(const PLLController&) = delete;
    PLLController& operator=(const PLLController&) = delete;

    void begin(CTFeedback* ct, PWMManager* pwm);
    void loop();

    State  getState() const { return _state; }
    float  getFrequency() const { return _freq; }
    float  getPhaseError() const { return _phaseError; }
    float  getIntegral() const { return _integral; }

    void setEnabled(bool v) { enabled = v; }
    bool isEnabled() const { return enabled; }
    void setPID(float kpVal, float kiVal) { kp = kpVal; ki = kiVal; }

    void reset();
    void setTarget(float freq) { if (_state == IDLE) _freq = freq; }

private:
    bool enabled = true;
    float kp = 0.5f;
    float ki = 0.02f;

    CTFeedback* _ct = nullptr;
    PWMManager* _pwm = nullptr;

    State  _state = IDLE;
    float  _freq = CoreParams::FREQ_MIN;
    float  _phaseError = 0.0f;
    float  _integral = 0.0f;

    float  _sweepFreq = CoreParams::FREQ_MAX;
    int    _lockCount = 0;
    int    _unlockCount = 0;
    static constexpr int LOCK_THRESHOLD = 3;
    static constexpr int UNLOCK_THRESHOLD = 5;

    uint32_t _lastZcdUs = 0;
    uint32_t _pwmSyncUs = 0;
    uint32_t _lastLoopUs = 0;
    uint32_t _lastSweepUs = 0;

    static constexpr uint32_t LOOP_INTERVAL_US = 500;
    static constexpr uint32_t SWEEP_INTERVAL_US = 2000;
    static constexpr float SWEEP_STEP_HZ = 300.0f;
    static constexpr float PI_CLAMP_HZ = 2000.0f;
    static constexpr float INTEGRAL_CLAMP = 5000.0f;

    void _sweepTick();
    void _pllTick(uint32_t zcdUs);
};
