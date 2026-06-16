#include "ConfigStore.h"

ConfigStore::ConfigStore() {}

ConfigStore::~ConfigStore() {}

bool ConfigStore::begin() {
    _prefs.begin(NS, false);
    _json = _prefs.getString(KEY, "");
    _prefs.end();

    if (_json.length() == 0) {
        _json = defaultJSON();
        saveParams(ParamValidator::defaults());
    }
    return true;
}

CoreParams ConfigStore::loadParams() {
    if (_json.length() == 0) {
        _json = defaultJSON();
    }
    return ParamValidator::clamp(parseParams(_json));
}

void ConfigStore::saveParams(const CoreParams& p) {
    // Always clamp before saving
    CoreParams clamped = ParamValidator::clamp(p);

    JsonDocument doc;
    doc["pwm"]["freq"]         = clamped.freq;
    doc["pwm"]["duty"]         = clamped.duty;
    doc["pwm"]["deadTimeNs"]   = clamped.deadTimeNs;
    doc["pwm"]["softStartMs"]  = clamped.softStartMs;
    doc["safety"]["runTimeoutSec"] = clamped.runTimeoutSec;
    doc["safety"]["enableTimeout"] = clamped.enableTimeout;
    doc["limits"]["freqMin"]   = clamped.freqMin;
    doc["limits"]["freqMax"]   = clamped.freqMax;
    doc["limits"]["dutyMin"]   = clamped.dutyMin;
    doc["limits"]["dutyMax"]   = clamped.dutyMax;
    doc["limits"]["dtMin"]     = clamped.dtMin;
    doc["limits"]["dtMax"]     = clamped.dtMax;
    doc["limits"]["ssMin"]     = clamped.ssMin;
    doc["limits"]["ssMax"]     = clamped.ssMax;

    _json = "";
    serializeJson(doc, _json);

    _prefs.begin(NS, false);
    _prefs.putString(KEY, _json);
    _prefs.end();
}

void ConfigStore::saveRaw(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return;

    CoreParams p = parseParams(json);
    saveParams(p);
}

String ConfigStore::toJSON() const {
    return _json;
}

void ConfigStore::resetToDefaults() {
    _json = defaultJSON();
    saveParams(ParamValidator::defaults());
}

WiFiCreds ConfigStore::loadWiFi() {
    WiFiCreds creds;
    Preferences p;
    p.begin(WIFI_NS, true);
    creds.ssid = p.getString(W_SSID, "");
    creds.pass = p.getString(W_PASS, "");
    p.end();
    return creds;
}

void ConfigStore::saveWiFi(const String& ssid, const String& pass) {
    Preferences p;
    p.begin(WIFI_NS, false);
    p.putString(W_SSID, ssid);
    p.putString(W_PASS, pass);
    p.end();
}

void ConfigStore::clearWiFi() {
    Preferences p;
    p.begin(WIFI_NS, false);
    p.clear();
    p.end();
}

bool ConfigStore::hasWiFiCreds() const {
    Preferences p;
    p.begin(WIFI_NS, true);
    String s = p.getString(W_SSID, "");
    p.end();
    return s.length() > 0;
}

// ── Private ──────────────────────────────────────────────────────

String ConfigStore::defaultJSON() const {
    return F("{"
        "\"pwm\":{\"freq\":100000,\"duty\":50.0,\"deadTimeNs\":500,\"softStartMs\":3000},"
        "\"safety\":{\"runTimeoutSec\":1800,\"enableTimeout\":true},"
        "\"limits\":{\"freqMin\":1000,\"freqMax\":400000,\"dutyMin\":0,\"dutyMax\":95,\"dtMin\":0,\"dtMax\":5000,\"ssMin\":500,\"ssMax\":10000}"
    "}");
}

CoreParams ConfigStore::parseParams(const String& json) const {
    CoreParams p;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return ParamValidator::defaults();

    p.freq          = doc["pwm"]["freq"]        | 100000.0f;
    p.duty          = doc["pwm"]["duty"]        | 50.0f;
    p.deadTimeNs    = doc["pwm"]["deadTimeNs"]  | 500.0f;
    p.softStartMs   = doc["pwm"]["softStartMs"] | 3000;
    p.runTimeoutSec = doc["safety"]["runTimeoutSec"] | 1800;
    p.enableTimeout = doc["safety"]["enableTimeout"] | true;
    p.freqMin       = doc["limits"]["freqMin"]  | 1000;
    p.freqMax       = doc["limits"]["freqMax"]  | 400000;
    p.dutyMin       = doc["limits"]["dutyMin"]  | 0.0f;
    p.dutyMax       = doc["limits"]["dutyMax"]  | 95.0f;
    p.dtMin         = doc["limits"]["dtMin"]    | 0.0f;
    p.dtMax         = doc["limits"]["dtMax"]    | 5000.0f;
    p.ssMin         = doc["limits"]["ssMin"]    | 500;
    p.ssMax         = doc["limits"]["ssMax"]    | 10000;

    return p;
}
