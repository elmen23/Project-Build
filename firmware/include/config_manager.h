#pragma once
/**
 * ═══════════════════════════════════════════════════════════════════
 *  config_manager.h  —  v3.0
 *  JSON configuration persisted in NVS.
 *  Provides default config, load/save, and Web UI endpoints.
 * ═══════════════════════════════════════════════════════════════════
 */

#include <Preferences.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#define CFG_NS        "device_cfg"
#define CFG_KEY_JSON  "json"

// Default JSON configuration (keep compact)
static const char DEFAULT_CONFIG_JSON[] PROGMEM =
"{"
  "\"pwm\":{\"frequency\":100000,\"duty\":50.0,\"dead_time_ns\":500,\"soft_start_ms\":3000},"
  "\"safety\":{\"run_timeout_sec\":1800,\"emergency_stop_pin\":23,\"enable_timeout\":true},"
  "\"limits\":{\"freq_min\":1000,\"freq_max\":400000,\"duty_min\":0,\"duty_max\":95,\"dt_min\":0,\"dt_max\":5000,\"ss_min\":500,\"ss_max\":10000},"
  "\"ui\":{\"refresh_interval_ms\":1000,\"theme\":\"dark\"}"
"}";

class ConfigManager {
public:
  String json;   // current config string

  bool begin() {
    prefs.begin(CFG_NS, false);
    json = prefs.getString(CFG_KEY_JSON, "");
    prefs.end();
    if (json.length() == 0) {
      json = String(DEFAULT_CONFIG_JSON);
      save();
    }
    return true;
  }

  void save() {
    prefs.begin(CFG_NS, false);
    prefs.putString(CFG_KEY_JSON, json);
    prefs.end();
  }

  void resetToDefaults() {
    json = String(DEFAULT_CONFIG_JSON);
    save();
  }

  // ─────────────────────────────────────────────────────────────
  //  Typed accessors using ArduinoJson (correct & whitespace-tolerant)
  // ─────────────────────────────────────────────────────────────

  float getFloat(const char* section, const char* key, float def) const {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return def;
    return doc[section][key] | def;
  }

  int getInt(const char* section, const char* key, int def) const {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return def;
    return doc[section][key] | def;
  }

  bool getBool(const char* section, const char* key, bool def) const {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return def;
    return doc[section][key] | def;
  }

  void setFloat(const char* section, const char* key, float value) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) doc.clear();
    doc[section][key] = value;
    json = "";
    serializeJson(doc, json);
  }

  void setInt(const char* section, const char* key, int value) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) doc.clear();
    doc[section][key] = value;
    json = "";
    serializeJson(doc, json);
  }

  void setBool(const char* section, const char* key, bool value) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) doc.clear();
    doc[section][key] = value;
    json = "";
    serializeJson(doc, json);
  }

private:
  Preferences prefs;
};
