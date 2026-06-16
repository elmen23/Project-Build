#pragma once
/**
 * ═══════════════════════════════════════════════════════════════════
 *  wifi_manager.h  —  v2.1 Enhanced
 *  Robust WiFi connection manager for ESP32 with dual-mode support:
 *  • AP provisioning mode (captive portal style)
 *  • STA client mode with persistent credentials (NVS)
 *  • Auto-retry with exponential cooldown
 *  • Seamless AP↔STA transitions
 * ═══════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <Preferences.h>

// ── AP Configuration ────────────────────────────────────────────
#define WIFI_AP_SSID     "Induction_Setup"
#define WIFI_AP_PASS     "12345678"
#define WIFI_AP_IP       "192.168.4.1"

// ── Timing & Retry Policy ───────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS    30000   // 30s per attempt
#define WIFI_MAX_RETRIES           3       // initial provisioning retries
#define WIFI_FAIL_COOLDOWN_MS      120000  // 2 min before auto-retry
#define WIFI_RECONNECT_INTERVAL_MS 30000   // 30s between reconnect attempts

// ── NVS Keys ────────────────────────────────────────────────────
#define NVS_NS        "wifi_cfg"
#define NVS_KEY_SSID  "ssid"
#define NVS_KEY_PASS  "pass"

// ── State Machine ───────────────────────────────────────────────
enum WiFiState {
  WIFI_STATE_INIT = 0,
  WIFI_STATE_AP_ONLY,
  WIFI_STATE_CONNECTING,
  WIFI_STATE_CONNECTED
};

class WiFiManager {
public:
  WiFiState state          = WIFI_STATE_INIT;

  // Persisted credentials
  String  savedSSID;
  String  savedPass;
  bool    hasCredentials   = false;

  // Runtime state
  String  staIP;
  String  connectingSSID;
  int     retryCount       = 0;
  bool    wasConnected     = false;

  // Timing trackers
  uint32_t connectStartMs  = 0;
  uint32_t failedAtMs      = 0;
  uint32_t lastReconnectMs = 0;

  Preferences prefs;

  // ─────────────────────────────────────────────────────────────
  //  Credential Persistence
  // ─────────────────────────────────────────────────────────────

  bool loadCredentials() {
    prefs.begin(NVS_NS, true);  // read-only
    savedSSID = prefs.getString(NVS_KEY_SSID, "");
    savedPass = prefs.getString(NVS_KEY_PASS, "");
    prefs.end();

    // Validate: SSID must be 1..32 chars
    hasCredentials = (savedSSID.length() > 0 && savedSSID.length() <= 32);

    if (hasCredentials) {
      Serial.printf("[WiFiMgr] Credentials loaded: SSID='%s'\n", savedSSID.c_str());
    } else {
      Serial.println("[WiFiMgr] No saved credentials found");
    }
    return hasCredentials;
  }

  void saveCredentials(const String& ssid, const String& pass) {
    if (ssid.length() == 0 || ssid.length() > 32) {
      Serial.println("[WiFiMgr] Invalid SSID, not saving");
      return;
    }
    prefs.begin(NVS_NS, false);  // read-write
    prefs.putString(NVS_KEY_SSID, ssid);
    prefs.putString(NVS_KEY_PASS, pass);
    prefs.end();

    savedSSID      = ssid;
    savedPass      = pass;
    hasCredentials = true;
    Serial.printf("[WiFiMgr] Credentials saved: SSID='%s'\n", ssid.c_str());
  }

  void clearCredentials() {
    prefs.begin(NVS_NS, false);
    prefs.clear();
    prefs.end();
    savedSSID = "";
    savedPass = "";
    hasCredentials = false;
    Serial.println("[WiFiMgr] Credentials cleared");
  }

  // ─────────────────────────────────────────────────────────────
  //  Mode Switching
  // ─────────────────────────────────────────────────────────────

  void startAP() {
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAPConfig(
      IPAddress(192,168,4,1),
      IPAddress(192,168,4,1),
      IPAddress(255,255,255,0)
    );
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, 6, 0, 4);  // channel 6, max 4 clients
    delay(200);
    Serial.printf("[WiFiMgr] AP mode: '%s' @ %s (ch 6)\n",
                  WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    state = WIFI_STATE_AP_ONLY;
  }

  void startAPSTA(const String& ssid, const String& pass, int channel = 1) {
    connectingSSID = ssid;
    state = WIFI_STATE_CONNECTING;

    WiFi.mode(WIFI_AP_STA);
    delay(100);
    WiFi.softAPConfig(
      IPAddress(192,168,4,1),
      IPAddress(192,168,4,1),
      IPAddress(255,255,255,0)
    );

    int apChannel = (channel >= 1 && channel <= 13) ? channel : 1;
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, apChannel, 0, 4);
    delay(200);

    WiFi.begin(ssid.c_str(), pass.c_str());
    connectStartMs = millis();
    retryCount++;

    Serial.printf("[WiFiMgr] AP+STA: AP on ch %d, connecting to '%s' (attempt %d/%d)...\n",
                  apChannel, ssid.c_str(), retryCount, WIFI_MAX_RETRIES);
  }

  // ─────────────────────────────────────────────────────────────
  //  Status Serialization
  // ─────────────────────────────────────────────────────────────

  String getStatusJSON() const {
    const char* stateStr;
    switch (state) {
      case WIFI_STATE_AP_ONLY:    stateStr = "ap_only";    break;
      case WIFI_STATE_CONNECTING: stateStr = "connecting"; break;
      case WIFI_STATE_CONNECTED:  stateStr = "connected";  break;
      default:                    stateStr = "init";       break;
    }

    uint32_t elapsed   = (state == WIFI_STATE_CONNECTING)
                           ? (millis() - connectStartMs) : 0;
    uint32_t remaining = (elapsed < WIFI_CONNECT_TIMEOUT_MS)
                           ? (WIFI_CONNECT_TIMEOUT_MS - elapsed) / 1000 : 0;

    char buf[300];
    snprintf(buf, sizeof(buf),
      "{\"state\":\"%s\","
      "\"ssid\":\"%s\","
      "\"ip\":\"%s\","
      "\"rssi\":%d,"
      "\"retry\":%d,"
      "\"remainSec\":%u,"
      "\"apClients\":%d}",
      stateStr,
      (state == WIFI_STATE_CONNECTED) ? savedSSID.c_str()
        : (state == WIFI_STATE_CONNECTING) ? connectingSSID.c_str() : "",
      staIP.c_str(),
      (state == WIFI_STATE_CONNECTED) ? WiFi.RSSI() : 0,
      retryCount,
      remaining,
      (state == WIFI_STATE_AP_ONLY || state == WIFI_STATE_CONNECTING)
        ? WiFi.softAPgetStationNum() : 0
    );
    return String(buf);
  }

  // ─────────────────────────────────────────────────────────────
  //  State Machine Tick — call from loop()
  // ─────────────────────────────────────────────────────────────

  bool tick() {
    switch (state) {

      // ═══ CONNECTING ═══
      case WIFI_STATE_CONNECTING: {
        // Success path
        if (WiFi.status() == WL_CONNECTED) {
          staIP        = WiFi.localIP().toString();
          state        = WIFI_STATE_CONNECTED;
          retryCount   = 0;
          wasConnected = true;
          failedAtMs   = 0;

          Serial.printf("[WiFiMgr] ✓ Connected! IP=%s, RSSI=%d dBm\n",
                        staIP.c_str(), WiFi.RSSI());
          return true;
        }

        // Timeout check
        if (millis() - connectStartMs >= WIFI_CONNECT_TIMEOUT_MS) {
          WiFi.disconnect(false, false);
          Serial.printf("[WiFiMgr] ✗ Timeout after %d ms\n", WIFI_CONNECT_TIMEOUT_MS);

          if (wasConnected) {
            // We were connected before — this is a transient disconnect.
            // Retry indefinitely with backoff.
            if (millis() - lastReconnectMs >= WIFI_RECONNECT_INTERVAL_MS) {
              lastReconnectMs = millis();
              startAPSTA(savedSSID, savedPass);
            }
          }
          else if (retryCount < WIFI_MAX_RETRIES) {
            // Initial provisioning: retry STA
            delay(500);
            startAPSTA(savedSSID, savedPass);
          }
          else {
            // Max retries exhausted → pure AP fallback with cooldown timer
            retryCount = 0;
            failedAtMs = millis();
            startAP();
            Serial.println("[WiFiMgr] → AP fallback (auto-retry in 2 min)");
          }
          return true;
        }
        break;
      }

      // ═══ CONNECTED ═══
      case WIFI_STATE_CONNECTED: {
        if (WiFi.status() != WL_CONNECTED) {
          Serial.println("[WiFiMgr] Connection lost!");
          retryCount   = 0;
          wasConnected = true;
          lastReconnectMs = millis();
          startAPSTA(savedSSID, savedPass);
          return true;
        }
        break;
      }

      // ═══ AP_ONLY ═══
      case WIFI_STATE_AP_ONLY: {
        // Auto-retry after cooldown period
        if (hasCredentials && failedAtMs > 0 &&
            (millis() - failedAtMs >= WIFI_FAIL_COOLDOWN_MS)) {
          Serial.println("[WiFiMgr] Cooldown expired, retrying...");
          failedAtMs = 0;
          startAPSTA(savedSSID, savedPass);
          return true;
        }
        break;
      }

      default: break;
    }
    return false;
  }
};
