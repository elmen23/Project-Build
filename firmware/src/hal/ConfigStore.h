#pragma once
#include <Preferences.h>
#include <Arduino.h>
#include "core/Types.h"

class ConfigStore {
public:
    CoreParams load();
    void save(const CoreParams& p);

    static constexpr const char* NS = "ih";
    static constexpr const char* K_FREQ = "freq";
    static constexpr const char* K_DUTY = "duty";
    static constexpr const char* K_DT = "dt";
    static constexpr const char* K_SS = "ss";
};
