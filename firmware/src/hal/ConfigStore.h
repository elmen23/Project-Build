#pragma once
#include <Preferences.h>
#include <Arduino.h>
#include "core/Types.h"

class ConfigStore {
public:
    void begin();
    CoreParams load();
    void save(const CoreParams& p);
    void reset();
    String toJSON(const CoreParams& p) const;

private:
    Preferences _prefs;

    static constexpr const char* NS = "device";
    static constexpr const char* K_FREQ = "freq";
    static constexpr const char* K_DUTY = "duty";
    static constexpr const char* K_DT = "dt";
    static constexpr const char* K_SS = "ss";
};
