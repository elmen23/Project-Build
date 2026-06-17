#pragma once
#include <Arduino.h>

class TimerMillis {
public:
    void start() { _start = millis(); }
    uint32_t elapsed() const { return millis() - _start; }
    bool hasPassed(uint32_t ms) const { return elapsed() >= ms; }
    void reset() { _start = millis(); }
private:
    uint32_t _start = 0;
};
