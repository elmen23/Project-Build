#pragma once
#include <Preferences.h>
#include <ArduinoJson.h>
#include "core/Types.h"
#include "core/ParamValidator.h"

class ConfigStore {
public:
    ConfigStore();
    ~ConfigStore();

    bool begin();

    CoreParams loadParams();
    void saveParams(const CoreParams& p);
    void saveRaw(const String& json);
    String toJSON() const;
    void resetToDefaults();

    WiFiCreds loadWiFi();
    void saveWiFi(const String& ssid, const String& pass);
    void clearWiFi();
    bool hasWiFiCreds() const;

private:
    Preferences _prefs;
    String _json;

    static constexpr const char* NS       = "device_cfg";
    static constexpr const char* KEY      = "json";
    static constexpr const char* WIFI_NS  = "wifi_cfg";
    static constexpr const char* W_SSID   = "ssid";
    static constexpr const char* W_PASS   = "pass";

    String defaultJSON() const;
    CoreParams parseParams(const String& json) const;
};
