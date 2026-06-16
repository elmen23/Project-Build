#include "ConfigStore.h"

void ConfigStore::begin() {
    _prefs.begin(NS, false);
}

CoreParams ConfigStore::load() {
    CoreParams p;
    p.freq = _prefs.getFloat(K_FREQ, 100000.0f);
    p.duty = _prefs.getFloat(K_DUTY, 50.0f);
    p.deadTimeNs = _prefs.getFloat(K_DT, 500.0f);
    p.softStartMs = _prefs.getUInt(K_SS, 3000);
    return p;
}

void ConfigStore::save(const CoreParams& p) {
    _prefs.putFloat(K_FREQ, p.freq);
    _prefs.putFloat(K_DUTY, p.duty);
    _prefs.putFloat(K_DT, p.deadTimeNs);
    _prefs.putUInt(K_SS, p.softStartMs);
    Serial.println(F("[Config] saved"));
}

void ConfigStore::reset() {
    _prefs.clear();
    Serial.println(F("[Config] reset"));
}

String ConfigStore::toJSON(const CoreParams& p) const {
    String j;
    j.reserve(128);
    j += "{\"freq\":";
    j += String(p.freq, 0);
    j += ",\"duty\":";
    j += String(p.duty, 1);
    j += ",\"dt\":";
    j += String(p.deadTimeNs, 0);
    j += ",\"ss\":";
    j += String(p.softStartMs);
    j += "}";
    return j;
}
