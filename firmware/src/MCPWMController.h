#pragma once
#include <Arduino.h>
#include "driver/mcpwm.h"

/* ─── Pinout ─── */
#define GPIO_PWM_HIGH  GPIO_NUM_18   /* High-side MOSFET */
#define GPIO_PWM_LOW   GPIO_NUM_19   /* Low-side MOSFET  */

class MCPWMController {
public:
    esp_err_t begin(float freq_hz = 50000.0f, float dead_time_ns = 500.0f);

    void start();
    void stop();
    void setFrequency(float hz);
    void setDeadTime(float ns);

    bool   isRunning()  const { return _running; }
    float  getFrequency() const { return _freq; }
    float  getDeadTime()  const { return _dtNs; }

    /* limits */
    static constexpr uint32_t FREQ_MIN = 10000;
    static constexpr uint32_t FREQ_MAX = 300000;
    static constexpr float    DT_MIN   = 0.0f;
    static constexpr float    DT_MAX   = 5000.0f;

private:
    bool   _running = false;
    float  _freq    = 50000.0f;
    float  _dtNs    = 500.0f;

    uint32_t nsToTicks(float ns) const;
};
