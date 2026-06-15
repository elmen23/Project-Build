/**
 * ═══════════════════════════════════════════════════════════════════
 *  Induction Heater Controller v2.1 — ESP32 (Enhanced Build)
 * ═══════════════════════════════════════════════════════════════════
 *  Features:
 *  • MCPWM half-bridge driver with complementary outputs
 *  • Configurable frequency, duty, dead-time, soft-start
 *  • Dual-mode WiFi: AP provisioning + STA operation
 *  • RESTful HTTP API with full input validation
 *  • Thread-safe shared state with critical sections
 *  • Watchdog-style connection monitoring
 * ═══════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/mcpwm.h"

#include "wifi_manager.h"
#include "html_pages.h"

// ── Pin Configuration ───────────────────────────────────────────
#define PWM_PIN_A     GPIO_NUM_18
#define PWM_PIN_B     GPIO_NUM_19

// ── Parameter Limits ────────────────────────────────────────────
#define FREQ_MIN       1000.0f
#define FREQ_MAX     400000.0f
#define DUTY_MIN         0.0f
#define DUTY_MAX        95.0f
#define DT_MIN           0.0f
#define DT_MAX        5000.0f
#define SS_MIN         500.0f
#define SS_MAX       10000.0f

// ── Timing Constants ────────────────────────────────────────────
#define DT_CLOCK_NS    12.5f      // 80MHz APB clock tick
#define SOFT_STEPS     200        // Soft-start resolution steps

// ── Global Shared State (protected by s_mux) ────────────────────
static portMUX_TYPE   s_mux        = portMUX_INITIALIZER_UNLOCKED;
static volatile float g_freq       = 100000.0f;
static volatile float g_duty       = 50.0f;
static volatile float g_dt         = 500.0f;
static volatile float g_ss         = 3000.0f;
static volatile bool  g_running    = false;
static volatile bool  g_softStart  = false;
static volatile bool  g_started    = false;

// ── Subsystem Instances ─────────────────────────────────────────
static WiFiManager       wifiMgr;
static AsyncWebServer    server(80);

// ── WiFi Scan State ─────────────────────────────────────────────
static volatile bool     g_scanDone   = false;
static volatile bool     g_scanning   = false;
static String            g_scanResult = "[]";
static SemaphoreHandle_t g_scanMutex  = NULL;

// ═════════════════════════════════════════════════════════════════
//  Helper: nanoseconds → MCPWM dead-time ticks
// ═════════════════════════════════════════════════════════════════
static inline uint32_t nsToTicks(float ns) {
  if (ns <= 0.0f) return 0;
  return (uint32_t)((ns / DT_CLOCK_NS) + 0.5f);
}

// ═════════════════════════════════════════════════════════════════
//  PWM Low-Level Interface
// ═════════════════════════════════════════════════════════════════

static void applyDeadTime(float ns) {
  uint32_t t = nsToTicks(ns);
  if (t > 0) {
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
                          MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE, t, t);
    Serial.printf("[PWM] Dead-time: %.0f ns → %u ticks\n", ns, t);
  } else {
    mcpwm_deadtime_disable(MCPWM_UNIT_0, MCPWM_TIMER_0);
    Serial.println("[PWM] Dead-time disabled");
  }
}

static void applyDuty(float pct) {
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, pct);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, pct);
}

static void stopPWMOutput() {
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0.0f);
  mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0.0f);
  mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
}

static void startPWMOutput() {
  mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
}

// ═════════════════════════════════════════════════════════════════
//  Soft-Start Task (FreeRTOS)
// ═════════════════════════════════════════════════════════════════

static void softStartTask(void* pv) {
  // Snapshot target parameters under lock
  float target, ms;
  portENTER_CRITICAL(&s_mux);
  target = g_duty;
  ms     = g_ss;
  portEXIT_CRITICAL(&s_mux);

  float stepSize = target / SOFT_STEPS;
  int   stepDly  = max(1, (int)(ms / SOFT_STEPS));

  Serial.printf("[SoftStart] Starting ramp: %.1f%% over %.0f ms (%d steps)\n",
                target, ms, SOFT_STEPS);

  for (int i = 1; i <= SOFT_STEPS; i++) {
    applyDuty(stepSize * i);
    vTaskDelay(pdMS_TO_TICKS(stepDly));
  }
  applyDuty(target);

  portENTER_CRITICAL(&s_mux);
  g_softStart = false;
  g_running   = true;
  portEXIT_CRITICAL(&s_mux);

  Serial.println("[SoftStart] Ramp complete, running.");
  vTaskDelete(NULL);
}

// ═════════════════════════════════════════════════════════════════
//  Control: Start / Stop
// ═════════════════════════════════════════════════════════════════

static bool startPWM() {
  portENTER_CRITICAL(&s_mux);
  if (g_started || g_softStart) {
    portEXIT_CRITICAL(&s_mux);
    Serial.println("[Control] Already started, ignoring.");
    return false;
  }
  g_softStart = true;
  g_started   = true;
  portEXIT_CRITICAL(&s_mux);

  startPWMOutput();
  xTaskCreate(softStartTask, "SoftStart", 3072, NULL, 5, NULL);
  Serial.println("[Control] Soft-start initiated.");
  return true;
}

static void stopPWM() {
  portENTER_CRITICAL(&s_mux);
  g_running   = false;
  g_softStart = false;
  g_started   = false;
  portEXIT_CRITICAL(&s_mux);

  stopPWMOutput();
  Serial.println("[Control] PWM stopped.");
}

// ═════════════════════════════════════════════════════════════════
//  WiFi Scanning (async via dedicated task)
// ═════════════════════════════════════════════════════════════════

static void scanTask(void* pv) {
  int n = WiFi.scanNetworks(false, false, false, 300);
  String json = "[";
  if (n > 0) {
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      String ssid = WiFi.SSID(i);
      // Escape JSON special characters
      ssid.replace("\\", "\\\\");
      ssid.replace("\"", "\\\"");
      ssid.replace("\b", "\\b");
      ssid.replace("\f", "\\f");
      ssid.replace("\n", "\\n");
      ssid.replace("\r", "\\r");
      ssid.replace("\t", "\\t");
      bool open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      char buf[192];
      snprintf(buf, sizeof(buf),
               "{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s,\"ch\":%d}",
               ssid.c_str(), WiFi.RSSI(i), open ? "true" : "false",
               (int)WiFi.channel(i));
      json += buf;
    }
  }
  json += "]";
  WiFi.scanDelete();

  if (xSemaphoreTake(g_scanMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
    g_scanResult = json;
    g_scanDone   = true;
    g_scanning   = false;
    xSemaphoreGive(g_scanMutex);
  }
  vTaskDelete(NULL);
}

static void triggerScan() {
  if (xSemaphoreTake(g_scanMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    bool already = g_scanning;
    if (!already) { g_scanning = true; g_scanDone = false; }
    xSemaphoreGive(g_scanMutex);
    if (!already) xTaskCreate(scanTask, "WiFiScan", 4096, NULL, 4, NULL);
  }
}

// ═════════════════════════════════════════════════════════════════
//  URL Decode helper
// ═════════════════════════════════════════════════════════════════
static String urlDecode(const String& src) {
  String out;
  out.reserve(src.length());
  for (int i = 0; i < (int)src.length(); i++) {
    if (src[i] == '%' && i + 2 < (int)src.length()) {
      char hex[3] = { src[i+1], src[i+2], 0 };
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    } else if (src[i] == '+') {
      out += ' ';
    } else {
      out += src[i];
    }
  }
  return out;
}

// ═════════════════════════════════════════════════════════════════
//  MCPWM Initialization
// ═════════════════════════════════════════════════════════════════

static void setupPWM() {
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PWM_PIN_A);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PWM_PIN_B);

  mcpwm_config_t cfg = {};
  cfg.frequency    = (uint32_t)g_freq;
  cfg.cmpr_a       = 0.0f;          // Safety: start at 0%
  cfg.cmpr_b       = 0.0f;
  cfg.counter_mode = MCPWM_UP_COUNTER;
  cfg.duty_mode    = MCPWM_DUTY_MODE_0;

  esp_err_t err = mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
  if (err != ESP_OK) {
    Serial.printf("[PWM] MCPWM init failed: %d\n", (int)err);
  }

  applyDeadTime(g_dt);
  // Do NOT start here — start only on user request
  Serial.println("[PWM] Initialized (outputs idle)");
}

// ═════════════════════════════════════════════════════════════════
//  HTTP Routes
// ═════════════════════════════════════════════════════════════════

static void setupRoutes() {
  // ── Root: serve control page if connected, setup page otherwise ──
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    const char* page = (wifiMgr.state == WIFI_STATE_CONNECTED)
                         ? INDEX_HTML : SETUP_HTML;
    req->send_P(200, "text/html", page);
  });

  // ── WiFi Scan ──
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
    String result = "[]";
    bool done = false, scanning = false;
    if (xSemaphoreTake(g_scanMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      done     = g_scanDone;
      scanning = g_scanning;
      if (done) { result = g_scanResult; g_scanDone = false; }
      xSemaphoreGive(g_scanMutex);
    }
    if (done) {
      req->send(200, "application/json", result);
    } else {
      if (!scanning) triggerScan();
      req->send(202, "application/json", "{\"scanning\":true}");
    }
  });

  // ── WiFi Connect (body handler for POST data) ──
  server.on("/connect", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      req->send(200, "text/plain", "OK");
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len,
       size_t index, size_t total) {
      static String bodyBuf;
      if (index == 0) bodyBuf = "";
      bodyBuf += String((const char*)data, len);
      if (index + len < total) return; // wait for full body

      // Parse application/x-www-form-urlencoded
      auto getParam = [&](const String& key) -> String {
        int s = bodyBuf.indexOf(key + "=");
        if (s < 0) return "";
        s += key.length() + 1;
        int e = bodyBuf.indexOf('&', s);
        String val = (e < 0) ? bodyBuf.substring(s) : bodyBuf.substring(s, e);
        return urlDecode(val);
      };

      String ssid = getParam("ssid");
      String pass = getParam("pass");

      if (ssid.length() > 0 && ssid.length() <= 32) {
        wifiMgr.saveCredentials(ssid, pass);
        wifiMgr.retryCount  = 0;
        wifiMgr.wasConnected = false;
        wifiMgr.startAPSTA(ssid, pass);
        Serial.printf("[Web] Connect request: SSID='%s'\n", ssid.c_str());
      } else {
        Serial.println("[Web] Invalid SSID in connect request");
      }
      bodyBuf = "";
    }
  );

  // ── WiFi Status ──
  server.on("/wifi-status", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", wifiMgr.getStatusJSON());
  });

  // ── Full System Status ──
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    portENTER_CRITICAL(&s_mux);
    float f  = g_freq;
    float d  = g_duty;
    float dt = g_dt;
    float ss = g_ss;
    bool  ru = g_running;
    bool  so = g_softStart;
    bool  st = g_started;
    portEXIT_CRITICAL(&s_mux);

    char buf[400];
    snprintf(buf, sizeof(buf),
      "{\"frequency\":%.1f,"
      "\"duty\":%.2f,"
      "\"deadTime\":%.1f,"
      "\"softStartMs\":%.1f,"
      "\"running\":%s,"
      "\"softStarting\":%s,"
      "\"started\":%s,"
      "\"ssid\":\"%s\","
      "\"ip\":\"%s\","
      "\"rssi\":%d}",
      f, d, dt, ss,
      ru  ? "true" : "false",
      so  ? "true" : "false",
      st  ? "true" : "false",
      wifiMgr.savedSSID.c_str(),
      wifiMgr.staIP.c_str(),
      (wifiMgr.state == WIFI_STATE_CONNECTED) ? WiFi.RSSI() : 0
    );
    req->send(200, "application/json", buf);
  });

  // ── Set Parameters ──
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest* req) {
    // Frequency
    if (req->hasParam("f")) {
      float v = req->getParam("f")->value().toFloat();
      if (v >= FREQ_MIN && v <= FREQ_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_freq = v;
        bool wasRunning = g_running;
        portEXIT_CRITICAL(&s_mux);

        mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);
        mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, (uint32_t)v);
        if (wasRunning) {
          portENTER_CRITICAL(&s_mux);
          float d = g_duty;
          portEXIT_CRITICAL(&s_mux);
          applyDuty(d);
          mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
        }
      }
    }

    // Duty
    if (req->hasParam("d")) {
      float v = req->getParam("d")->value().toFloat();
      if (v >= DUTY_MIN && v <= DUTY_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_duty    = v;
        bool starting = g_softStart;
        bool runFlag  = g_running;
        portEXIT_CRITICAL(&s_mux);
        if (!starting && runFlag) applyDuty(v);
      }
    }

    // Dead Time
    if (req->hasParam("dt")) {
      float v = req->getParam("dt")->value().toFloat();
      if (v >= DT_MIN && v <= DT_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_dt = v;
        portEXIT_CRITICAL(&s_mux);
        applyDeadTime(v);
      }
    }

    // Soft Start Duration
    if (req->hasParam("ss")) {
      float v = req->getParam("ss")->value().toFloat();
      if (v >= SS_MIN && v <= SS_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_ss = v;
        portEXIT_CRITICAL(&s_mux);
      }
    }

    req->send(200, "text/plain", "OK");
  });

  // ── Start / Stop ──
  server.on("/start", HTTP_GET, [](AsyncWebServerRequest* req) {
    bool ok = startPWM();
    req->send(200, "application/json",
              ok ? "{\"success\":true}" : "{\"success\":false,\"reason\":\"already_started\"}");
  });

  server.on("/stop", HTTP_GET, [](AsyncWebServerRequest* req) {
    stopPWM();
    req->send(200, "application/json", "{\"success\":true}");
  });

  // ── Reset WiFi & Reboot ──
  server.on("/reset-wifi", HTTP_POST, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "OK");
    stopPWM();
    Serial.println("[Web] WiFi reset requested, rebooting...");
    xTaskCreate([](void*) {
      vTaskDelay(pdMS_TO_TICKS(800));
      Preferences p;
      p.begin("wifi_cfg", false);
      p.clear();
      p.end();
      ESP.restart();
    }, "RebootTask", 2048, nullptr, 1, nullptr);
  });

  // ── 404 ──
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "application/json", "{\"error\":\"Not found\"}");
  });
}

// ═════════════════════════════════════════════════════════════════
//  Arduino Setup & Loop
// ═════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║   Induction Heater v2.1 — Enhanced Build ║");
  Serial.println("║   ESP32 MCPWM Half-Bridge Controller     ║");
  Serial.println("╚══════════════════════════════════════════╝");

  g_scanMutex = xSemaphoreCreateMutex();
  if (!g_scanMutex) {
    Serial.println("[FATAL] Failed to create scan mutex");
  }

  setupPWM();

  WiFi.setAutoReconnect(false);
  WiFi.persistent(false);

  if (wifiMgr.loadCredentials()) {
    wifiMgr.retryCount   = 0;
    wifiMgr.wasConnected = false;
    wifiMgr.startAPSTA(wifiMgr.savedSSID, wifiMgr.savedPass);
  } else {
    wifiMgr.startAP();
    triggerScan();
  }

  setupRoutes();
  server.begin();

  Serial.printf("[Web] AP active: http://%s\n", WIFI_AP_IP);
}

void loop() {
  wifiMgr.tick();

  static WiFiState lastState = WIFI_STATE_INIT;
  if (wifiMgr.state != lastState) {
    lastState = wifiMgr.state;
    if (lastState == WIFI_STATE_AP_ONLY) {
      triggerScan();
    }
    if (lastState == WIFI_STATE_CONNECTED) {
      Serial.printf("[WiFi] Connected, STA IP: %s\n", wifiMgr.staIP.c_str());
    }
    if (lastState == WIFI_STATE_CONNECTING) {
      Serial.println("[WiFi] Connecting...");
    }
  }

  vTaskDelay(pdMS_TO_TICKS(50));
}
