#pragma once
#include <Arduino.h>
#include "driver/gpio.h"

class CTFeedback {
public:
    CTFeedback();
    ~CTFeedback() = default;

    CTFeedback(const CTFeedback&) = delete;
    CTFeedback& operator=(const CTFeedback&) = delete;

    void begin();
    void loop();

    float    getTankFrequency()  const { return _tankFreq; }
    float    getTankAmplitude()  const { return _tankAmp; }
    bool     hasSignal()         const { return _hasSignal; }
    uint32_t getEdgeCount()      const { return _edgeCount; }
    uint32_t getLastEdgeTimeUs() const { return _lastEdgeTimeUs; }

    void enableZCD();
    void disableZCD();

    void setEnabled(bool v);
    bool isEnabled() const { return _enabled; }

    static constexpr gpio_num_t ADC_GPIO = GPIO_NUM_34;
    static constexpr gpio_num_t ZCD_GPIO = GPIO_NUM_22;

private:
    bool      _enabled = false;

    volatile uint32_t _lastEdgeTimeUs = 0;
    volatile uint32_t _prevEdgeTimeUs = 0;
    volatile uint32_t _lastPeriod = 0;
    volatile float    _tankFreq = 0.0f;
    volatile bool     _hasSignal = false;
    volatile uint32_t _edgeCount = 0;

    float    _tankAmp = 0.0f;
    uint32_t _lastAdcMs = 0;
    uint32_t _processedEdges = 0;
    uint32_t _lastSignalMs = 0;

    static constexpr uint32_t ADC_INTERVAL_MS = 10;

    static void IRAM_ATTR _zcdIsr(void* arg);
};
