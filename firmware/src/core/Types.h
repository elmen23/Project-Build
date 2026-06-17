#pragma once
#include <Arduino.h>
#include <cstdint>

enum class RunState : uint8_t {
    IDLE,
    SOFT_START,
    RUNNING
};

inline const char* runStateName(RunState s) {
    switch (s) {
        case RunState::IDLE:       return "IDLE";
        case RunState::SOFT_START: return "SOFT_START";
        case RunState::RUNNING:    return "RUNNING";
    }
    return "?";
}

struct CoreParams {
    float freq = 25000.0f;
    float deadTimeNs = 500.0f;
    uint32_t softStartMs = 1000;

    static constexpr CoreParams defaults() {
        return CoreParams();
    }

    static constexpr uint32_t FREQ_MIN = 20000;
    static constexpr uint32_t FREQ_MAX = 250000;
    static constexpr float DT_MIN = 0.0f;
    static constexpr float DT_MAX = 5000.0f;
    static constexpr uint32_t SS_MIN = 500;
    static constexpr uint32_t SS_MAX = 10000;
};
