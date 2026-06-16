#pragma once
#include <Arduino.h>
#include <cstdint>

enum class RunState : uint8_t {
    IDLE,
    SOFT_START,
    RUNNING,
    EMERGENCY_STOP
};

enum class WiFiState : uint8_t {
    INIT,
    AP_ONLY,
    CONNECTING,
    CONNECTED,
    FAILED
};

struct CoreParams {
    float freq = 100000.0f;
    float duty = 50.0f;
    float deadTimeNs = 500.0f;
    uint32_t softStartMs = 3000;
    uint32_t runTimeoutSec = 1800;
    bool enableTimeout = true;
    uint32_t freqMin = 1000;
    uint32_t freqMax = 400000;
    float dutyMin = 0.0f;
    float dutyMax = 95.0f;
    float dtMin = 0.0f;
    float dtMax = 5000.0f;
    uint32_t ssMin = 500;
    uint32_t ssMax = 10000;
};

struct WiFiCreds {
    String ssid;
    String pass;
    bool isEmpty() const { return ssid.length() == 0 || pass.length() == 0; }
};
