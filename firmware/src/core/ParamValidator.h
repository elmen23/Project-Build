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
        CoreParams def;
        if (isnan(r.freq) || isinf(r.freq))
            r.freq = def.freq;
        else
            r.freq = constrain(r.freq, (float)CoreParams::FREQ_MIN, (float)CoreParams::FREQ_MAX);
        if (isnan(r.duty) || isinf(r.duty))
            r.duty = def.duty;
        else
            r.duty = constrain(r.duty, CoreParams::DUTY_MIN, CoreParams::DUTY_MAX);
        if (isnan(r.deadTimeNs) || isinf(r.deadTimeNs))
            r.deadTimeNs = def.deadTimeNs;
        else
            r.deadTimeNs = constrain(r.deadTimeNs, CoreParams::DT_MIN, CoreParams::DT_MAX);
        r.softStartMs = constrain(r.softStartMs, CoreParams::SS_MIN, CoreParams::SS_MAX);
        return r;
    }

    static bool isValid(const CoreParams& p) {
        return p.freq >= CoreParams::FREQ_MIN && p.freq <= CoreParams::FREQ_MAX
            && p.duty >= CoreParams::DUTY_MIN && p.duty <= CoreParams::DUTY_MAX
            && p.deadTimeNs >= CoreParams::DT_MIN && p.deadTimeNs <= CoreParams::DT_MAX
            && p.softStartMs >= CoreParams::SS_MIN && p.softStartMs <= CoreParams::SS_MAX;
    }
};
