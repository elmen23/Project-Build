#include "ConfigStore.h"

CoreParams ConfigStore::load() {
    Preferences prefs;
    prefs.begin(NS, true);
    CoreParams p;
    CoreParams defs;
    p.freq = prefs.getFloat(K_FREQ, defs.freq);
    p.deadTimeNs = prefs.getFloat(K_DT, defs.deadTimeNs);
    p.softStartMs = prefs.getUInt(K_SS, defs.softStartMs);
    prefs.end();
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
