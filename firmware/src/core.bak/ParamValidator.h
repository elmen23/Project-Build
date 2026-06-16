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
        if (r.freq < r.freqMin || r.freq > r.freqMax || isnan(r.freq) || isinf(r.freq))
            r.freq = 100000.0f;
        if (r.duty < r.dutyMin || r.duty > r.dutyMax || isnan(r.duty) || isinf(r.duty))
            r.duty = 50.0f;
        if (r.deadTimeNs < r.dtMin || r.deadTimeNs > r.dtMax || isnan(r.deadTimeNs) || isinf(r.deadTimeNs))
            r.deadTimeNs = 500.0f;
        if (r.softStartMs < r.ssMin || r.softStartMs > r.ssMax)
            r.softStartMs = 3000;
        return r;
    }

    static bool isValid(const CoreParams& p) {
        return p.freq >= p.freqMin && p.freq <= p.freqMax
            && p.duty >= p.dutyMin && p.duty <= p.dutyMax
            && p.deadTimeNs >= p.dtMin && p.deadTimeNs <= p.dtMax
            && p.softStartMs >= p.ssMin && p.softStartMs <= p.ssMax;
    }
};
