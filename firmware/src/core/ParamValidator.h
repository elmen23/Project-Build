#pragma once
#include "Types.h"
#include <cmath>

class ParamValidator {
public:
    static CoreParams defaults() {
        return CoreParams();
    }

    static CoreParams clamp(const CoreParams& p) {
        CoreParams r = p;
        if (r.freq < CoreParams::FREQ_MIN || r.freq > CoreParams::FREQ_MAX || isnan(r.freq) || isinf(r.freq))
            r.freq = 100000.0f;
        if (r.duty < CoreParams::DUTY_MIN || r.duty > CoreParams::DUTY_MAX || isnan(r.duty) || isinf(r.duty))
            r.duty = 50.0f;
        if (r.deadTimeNs < CoreParams::DT_MIN || r.deadTimeNs > CoreParams::DT_MAX || isnan(r.deadTimeNs) || isinf(r.deadTimeNs))
            r.deadTimeNs = 500.0f;
        if (r.softStartMs < CoreParams::SS_MIN || r.softStartMs > CoreParams::SS_MAX)
            r.softStartMs = 3000;
        return r;
    }

    static bool isValid(const CoreParams& p) {
        return p.freq >= CoreParams::FREQ_MIN && p.freq <= CoreParams::FREQ_MAX
            && p.duty >= CoreParams::DUTY_MIN && p.duty <= CoreParams::DUTY_MAX
            && p.deadTimeNs >= CoreParams::DT_MIN && p.deadTimeNs <= CoreParams::DT_MAX
            && p.softStartMs >= CoreParams::SS_MIN && p.softStartMs <= CoreParams::SS_MAX;
    }
};
