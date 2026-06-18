#include "ConfigStore.h"

CoreParams ConfigStore::load() {
    CoreParams p;
    CoreParams defs;

    Preferences prefs;
    prefs.begin(NS, true);
    bool hasConfig = prefs.isKey(K_FREQ) || prefs.isKey(K_DT) || prefs.isKey(K_SS);
    p.freq = prefs.getFloat(K_FREQ, defs.freq);
    p.deadTimeNs = prefs.getFloat(K_DT, defs.deadTimeNs);
    p.softStartMs = prefs.getUInt(K_SS, defs.softStartMs);
    prefs.end();

    if (!hasConfig) {
        Preferences legacy;
        legacy.begin("ih", true);
        p.freq = legacy.getFloat(K_FREQ, p.freq);
        p.deadTimeNs = legacy.getFloat(K_DT, p.deadTimeNs);
        p.softStartMs = legacy.getUInt(K_SS, p.softStartMs);
        legacy.end();
    }

    return p;
}

void ConfigStore::save(const CoreParams& p) {
    Preferences prefs;
    prefs.begin(NS, false);
    prefs.putFloat(K_FREQ, p.freq);
    prefs.putFloat(K_DT, p.deadTimeNs);
    prefs.putUInt(K_SS, p.softStartMs);
    prefs.end();
    Serial.println(F("[Config] saved"));
}
