#pragma once
#include <Arduino.h>
#include <cstdint>

enum class RunState : uint8_t {
    IDLE,
    SOFT_START,
    RUNNING,
    STOPPING
};

struct CoreParams {
    float freq = 25000.0f;
    float duty = 50.0f;
    float deadTimeNs = 500.0f;
    uint32_t softStartMs = 1000;
    uint32_t runTimeoutSec = 1800;
    bool enableTimeout = true;

    static constexpr uint32_t FREQ_MIN = 1000;
    static constexpr uint32_t FREQ_MAX = 400000;
    static constexpr float DUTY_MIN = 0.0f;
    static constexpr float DUTY_MAX = 95.0f;
    static constexpr float DT_MIN = 0.0f;
    static constexpr float DT_MAX = 5000.0f;
    static constexpr uint32_t SS_MIN = 500;
    static constexpr uint32_t SS_MAX = 10000;
};
