/**
 * ═══════════════════════════════════════════════════════════════════
 *  Induction Heater Controller v3.0 — ESP32 (Real-time Edition)
 * ═══════════════════════════════════════════════════════════════════
 *  New in v3.0:
 *  • WebSocket real-time broadcast (replaces HTTP polling)
 *  • Emergency Stop (GPIO interrupt + Web UI)
 *  • Run Timeout (auto-stop after configurable duration)
 *  • Event Log (NVS circular buffer)
 *  • JSON Config File (editable via Web UI)
 * ═══════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/mcpwm.h"

#include "wifi_manager.h"
#include "event_logger.h"
#include "config_manager.h"
#include "html_pages.h"

// ── Pin Configuration ───────────────────────────────────────────
#define PWM_PIN_A         GPIO_NUM_18
#define PWM_PIN_B         GPIO_NUM_19
#define EMERGENCY_PIN     GPIO_NUM_23   // Active-LOW emergency stop button

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

// ── Run Timeout ─────────────────────────────────────────────────
static volatile uint32_t g_runStartMs    = 0;
static volatile uint32_t g_runTimeoutSec = 1800;  // default 30 min
static volatile bool     g_timeoutEnabled  = true;

// ── Cross-Task Communication Flags ──────────────────────────────
static volatile bool  g_emergencyPending   = false;  // set in ISR, consumed in loop()
static volatile bool  g_softStartComplete  = false;  // set in softStartTask, consumed in loop()

// ── Subsystem Instances ─────────────────────────────────────────
static WiFiManager       wifiMgr;
static ConfigManager     cfgMgr;
static EventLogger       evtLog;
static AsyncWebServer    server(80);
static AsyncWebSocket    ws("/ws");

// ── WiFi Scan State ─────────────────────────────────────────────
static volatile bool     g_scanDone   = false;
static volatile bool     g_scanning   = false;
static String            g_scanResult = "[]";
static SemaphoreHandle_t g_scanMutex  = NULL;

// ── Forward Declarations ────────────────────────────────────────
static void stopPWM();
static void broadcastStatus();
static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len);

// ═════════════════════════════════════════════════════════════════
//  Config Load / Apply
// ═════════════════════════════════════════════════════════════════

static void applyConfig() {
  g_freq   = cfgMgr.getFloat("pwm", "frequency",     100000.0f);
  g_duty   = cfgMgr.getFloat("pwm", "duty",          50.0f);
  g_dt     = cfgMgr.getFloat("pwm", "dead_time_ns",  500.0f);
  g_ss     = cfgMgr.getFloat("pwm", "soft_start_ms", 3000.0f);
  g_runTimeoutSec = cfgMgr.getInt("safety", "run_timeout_sec", 1800);
  g_timeoutEnabled = cfgMgr.getBool("safety", "enable_timeout", true);

  // Clamp
  if (g_freq < FREQ_MIN || g_freq > FREQ_MAX || isnan(g_freq) || isinf(g_freq)) g_freq = 100000.0f;
  if (g_duty < DUTY_MIN || g_duty > DUTY_MAX || isnan(g_duty) || isinf(g_duty)) g_duty = 50.0f;
  if (g_dt   < DT_MIN   || g_dt   > DT_MAX   || isnan(g_dt)   || isinf(g_dt))   g_dt   = 500.0f;
  if (g_ss   < SS_MIN   || g_ss   > SS_MAX   || isnan(g_ss)   || isinf(g_ss))   g_ss   = 3000.0f;
}

// ═════════════════════════════════════════════════════════════════
//  (Legacy NVS persistence removed — all config now goes through
//   ConfigManager. Parameters are persisted via cfgMgr.setFloat
//   et al. + cfgMgr.save() to a single NVS namespace.)
// ═════════════════════════════════════════════════════════════════

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
  // NOTE: mcpwm_set_duty_type() is called once in setupPWM(), not here
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
  g_runStartMs = millis();
  g_softStartComplete = true;   // main loop will log + broadcast
  portEXIT_CRITICAL(&s_mux);

  vTaskDelete(NULL);
}

// ═════════════════════════════════════════════════════════════════
//  Control: Start / Stop / Emergency
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
  evtLog.log(EVT_START, "PWM soft-start initiated");
  broadcastStatus();
  return true;
}

static void stopPWM() {
  portENTER_CRITICAL(&s_mux);
  bool wasRunning = g_running || g_softStart;
  g_running   = false;
  g_softStart = false;
  g_started   = false;
  portEXIT_CRITICAL(&s_mux);

  stopPWMOutput();
  if (wasRunning) {
    evtLog.log(EVT_STOP, "PWM stopped");
    broadcastStatus();
  }
  Serial.println("[Control] PWM stopped.");
}

static void emergencyStop() {
  portENTER_CRITICAL(&s_mux);
  bool wasRunning = g_running || g_softStart;
  g_running   = false;
  g_softStart = false;
  g_started   = false;
  portEXIT_CRITICAL(&s_mux);

  stopPWMOutput();
  if (wasRunning) {
    evtLog.log(EVT_EMERGENCY, "EMERGENCY STOP triggered");
    broadcastStatus();
  }
  Serial.println("[EMERGENCY] STOP triggered!");
}

// ═════════════════════════════════════════════════════════════════
//  Emergency Stop ISR
// ═════════════════════════════════════════════════════════════════

static void IRAM_ATTR onEmergencyISR() {
  // NOTE: MCPWM functions are NOT ISR-safe (they use spinlocks internally).
  // Do NOT call mcpwm_set_duty/mcpwm_stop from ISR — that can deadlock.
  //
  // Instead: set a flag for loop() and detach the interrupt (debounce).
  // The loop() will call emergencyStop() which safely stops MCPWM
  // from task context.
  g_emergencyPending = true;
  detachInterrupt(digitalPinToInterrupt(EMERGENCY_PIN));
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
//  Status JSON Builder
// ═════════════════════════════════════════════════════════════════

static String buildStatusJSON() {
  portENTER_CRITICAL(&s_mux);
  float f  = g_freq;
  float d  = g_duty;
  float dt = g_dt;
  float ss = g_ss;
  bool  ru = g_running;
  bool  so = g_softStart;
  bool  st = g_started;
  uint32_t runMs = (ru && g_runStartMs > 0) ? (millis() - g_runStartMs) : 0;
  portEXIT_CRITICAL(&s_mux);

  char buf[600];
  snprintf(buf, sizeof(buf),
    "{\"frequency\":%.1f,"
    "\"duty\":%.2f,"
    "\"deadTime\":%.1f,"
    "\"softStartMs\":%.1f,"
    "\"running\":%s,"
    "\"softStarting\":%s,"
    "\"started\":%s,"
    "\"runTimeMs\":%u,"
    "\"runTimeoutSec\":%u,"
    "\"enableTimeout\":%s,"
    "\"ssid\":\"%s\","
    "\"ip\":\"%s\","
    "\"rssi\":%d}",
    f, d, dt, ss,
    ru  ? "true" : "false",
    so  ? "true" : "false",
    st  ? "true" : "false",
    runMs,
    g_runTimeoutSec,
    g_timeoutEnabled ? "true" : "false",
    wifiMgr.savedSSID.c_str(),
    wifiMgr.staIP.c_str(),
    (wifiMgr.state == WIFI_STATE_CONNECTED) ? WiFi.RSSI() : 0
  );
  return String(buf);
}

// ═════════════════════════════════════════════════════════════════
//  WebSocket Broadcast
// ═════════════════════════════════════════════════════════════════

static void broadcastStatus() {
  String json = buildStatusJSON();
  ws.textAll(json);
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      client->text(buildStatusJSON());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA: {
      AwsFrameInfo *info = (AwsFrameInfo*)arg;
      if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        data[len] = 0;
        String msg = (char*)data;
        msg.trim();
        if (msg == "start") {
          startPWM();
        } else if (msg == "stop") {
          stopPWM();
        } else if (msg == "status") {
          client->text(buildStatusJSON());
        }
      }
      break;
    }
    default:
      break;
  }
}

// ═════════════════════════════════════════════════════════════════
//  MCPWM Initialization
// ═════════════════════════════════════════════════════════════════

static void setupPWM() {
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PWM_PIN_A);
  mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PWM_PIN_B);

  mcpwm_config_t cfg = {};
  cfg.frequency    = (uint32_t)g_freq;
  cfg.cmpr_a       = 0.0f;
  cfg.cmpr_b       = 0.0f;
  cfg.counter_mode = MCPWM_UP_COUNTER;
  cfg.duty_mode    = MCPWM_DUTY_MODE_0;

  esp_err_t err = mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &cfg);
  if (err != ESP_OK) {
    Serial.printf("[PWM] MCPWM init failed: %d\n", (int)err);
    evtLog.log(EVT_ERROR, "MCPWM init failed: %d", (int)err);
  }

  applyDeadTime(g_dt);

  // Set duty type once (not on every applyDuty call)
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, MCPWM_DUTY_MODE_0);
  mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, MCPWM_DUTY_MODE_0);

  Serial.println("[PWM] Initialized (outputs idle)");
}

// ═════════════════════════════════════════════════════════════════
//  HTTP Routes
// ═════════════════════════════════════════════════════════════════

static void setupRoutes() {
  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // ── Root ──
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    const char* page = (wifiMgr.state == WIFI_STATE_CONNECTED || wifiMgr.forceControlPanel)
                         ? INDEX_HTML : SETUP_HTML;
    req->send_P(200, "text/html", page);
  });

  // ── Force control panel in AP mode ──
  server.on("/use-ap", HTTP_POST, [](AsyncWebServerRequest* req) {
    wifiMgr.forceControlPanel = true;
    req->send(200, "application/json", "{\"success\":true,\"redirect\":\"/\"}");
    Serial.println("[Web] AP mode forced — control panel enabled");
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

  // ── WiFi Connect ──
  server.on("/connect", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      // req->_tempObject holds the accumulated body (String*)
      String* bodyBuf = static_cast<String*>(req->_tempObject);
      if (!bodyBuf) { req->send(400, "text/plain", "No body"); return; }

      auto getParam = [&](const String& key) -> String {
        int s = bodyBuf->indexOf(key + "=");
        if (s < 0) return "";
        s += key.length() + 1;
        int e = bodyBuf->indexOf('&', s);
        String val = (e < 0) ? bodyBuf->substring(s) : bodyBuf->substring(s, e);
        return urlDecode(val);
      };

      String ssid    = getParam("ssid");
      String pass    = getParam("pass");
      int    channel = getParam("ch").toInt();

      if (ssid.length() > 0 && ssid.length() <= 32) {
        wifiMgr.saveCredentials(ssid, pass);
        wifiMgr.retryCount   = 0;
        wifiMgr.wasConnected = false;
        wifiMgr.startAPSTA(ssid, pass, channel);
        evtLog.log(EVT_WIFI_CONNECT, "Connect request: %s", ssid.c_str());
        Serial.printf("[Web] Connect request: SSID='%s', pass_len=%d, ch=%d\n",
                      ssid.c_str(), pass.length(), channel);
      } else {
        Serial.println("[Web] Invalid SSID in connect request");
      }

      delete bodyBuf;
      req->_tempObject = nullptr;
      req->send(200, "text/plain", "OK");
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len,
       size_t index, size_t total) {
      if (index == 0) {
        req->_tempObject = new String();
      }
      String* buf = static_cast<String*>(req->_tempObject);
      if (buf) {
        buf->concat(reinterpret_cast<const char*>(data), len);
      }
    }
  );

  // ── WiFi Status ──
  server.on("/wifi-status", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", wifiMgr.getStatusJSON());
  });

  // ── Full System Status ──
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", buildStatusJSON());
  });

  // ── Set Parameters ──
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest* req) {
    bool changed = false;

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
        changed = true;
      }
    }

    if (req->hasParam("d")) {
      float v = req->getParam("d")->value().toFloat();
      if (v >= DUTY_MIN && v <= DUTY_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_duty = v;
        bool starting = g_softStart;
        bool runFlag  = g_running;
        portEXIT_CRITICAL(&s_mux);
        if (!starting && runFlag) applyDuty(v);
        changed = true;
      }
    }

    if (req->hasParam("dt")) {
      float v = req->getParam("dt")->value().toFloat();
      if (v >= DT_MIN && v <= DT_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_dt = v;
        portEXIT_CRITICAL(&s_mux);
        applyDeadTime(v);
        changed = true;
      }
    }

    if (req->hasParam("ss")) {
      float v = req->getParam("ss")->value().toFloat();
      if (v >= SS_MIN && v <= SS_MAX && !isnan(v) && !isinf(v)) {
        portENTER_CRITICAL(&s_mux);
        g_ss = v;
        portEXIT_CRITICAL(&s_mux);
        changed = true;
      }
    }

    // Persist via ConfigManager (single source of truth)
    portENTER_CRITICAL(&s_mux);
    float pf = g_freq, pd = g_duty, pdt = g_dt, pss = g_ss;
    portEXIT_CRITICAL(&s_mux);
    cfgMgr.setFloat("pwm", "frequency", pf);
    cfgMgr.setFloat("pwm", "duty", pd);
    cfgMgr.setFloat("pwm", "dead_time_ns", pdt);
    cfgMgr.setFloat("pwm", "soft_start_ms", pss);
    cfgMgr.save();

    if (changed) {
      evtLog.log(EVT_PARAM_CHANGE, "f=%.0f d=%.1f dt=%.0f ss=%.0f", pf, pd, pdt, pss);
      broadcastStatus();
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

  // ── Emergency Stop HTTP endpoint ──
  server.on("/emergency", HTTP_POST, [](AsyncWebServerRequest* req) {
    emergencyStop();
    req->send(200, "application/json", "{\"success\":true,\"emergency\":true}");
  });

  // ── Event Log ──
  server.on("/events", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", evtLog.toJSON());
  });

  server.on("/events/clear", HTTP_POST, [](AsyncWebServerRequest* req) {
    evtLog.clear();
    req->send(200, "application/json", "{\"success\":true}");
  });

  // ── Config File ──
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", cfgMgr.json);
  });

  server.on("/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      String* bodyBuf = static_cast<String*>(req->_tempObject);
      if (!bodyBuf) { req->send(400, "text/plain", "No body"); return; }

      cfgMgr.json = *bodyBuf;
      cfgMgr.save();
      applyConfig();
      evtLog.log(EVT_INFO, "Config updated");

      delete bodyBuf;
      req->_tempObject = nullptr;
      req->send(200, "text/plain", "OK");
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len,
       size_t index, size_t total) {
      if (index == 0) {
        req->_tempObject = new String();
      }
      String* buf = static_cast<String*>(req->_tempObject);
      if (buf) {
        buf->concat(reinterpret_cast<const char*>(data), len);
      }
    }
  );

  server.on("/config/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    cfgMgr.resetToDefaults();
    applyConfig();
    req->send(200, "application/json", cfgMgr.json);
  });

  // ── Reset WiFi & Reboot ──
  server.on("/reset-wifi", HTTP_POST, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "OK");
    stopPWM();
    evtLog.log(EVT_INFO, "WiFi reset requested");
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
//  WiFi Event Handler (diagnostics)
// ═════════════════════════════════════════════════════════════════

static void wifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFiEvent] STA CONNECTED (link layer up)");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFiEvent] GOT IP: %s\n", WiFi.localIP().toString().c_str());
      evtLog.log(EVT_WIFI_CONNECT, "STA IP: %s", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFiEvent] STA DISCONNECTED, status=%d\n", WiFi.status());
      evtLog.log(EVT_WIFI_DISCONNECT, "STA disconnected");
      break;
    default:
      break;
  }
}

// ═════════════════════════════════════════════════════════════════
//  Arduino Setup & Loop
// ═════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n╔══════════════════════════════════════════╗");
  Serial.println("║   Induction Heater v3.0 — Real-time      ║");
  Serial.println("║   ESP32 MCPWM Half-Bridge Controller     ║");
  Serial.println("╚══════════════════════════════════════════╝");

  // Init subsystems
  evtLog.begin();
  cfgMgr.begin();
  applyConfig();

  WiFi.onEvent(wifiEvent);

  g_scanMutex = xSemaphoreCreateMutex();
  if (!g_scanMutex) {
    Serial.println("[FATAL] Failed to create scan mutex");
  }

  // Emergency stop pin
  pinMode(EMERGENCY_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(EMERGENCY_PIN), onEmergencyISR, FALLING);
  Serial.printf("[Safety] Emergency stop on GPIO %d (active-LOW)\n", EMERGENCY_PIN);

  // (Config already loaded via cfgMgr.begin() + applyConfig() above)
  setupPWM();

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
  evtLog.log(EVT_INFO, "System boot completed");
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

  // ── Run Timeout Check ──
  if (g_timeoutEnabled && g_running) {
    uint32_t elapsedSec = (millis() - g_runStartMs) / 1000;
    if (elapsedSec >= g_runTimeoutSec) {
      stopPWM();
      evtLog.log(EVT_TIMEOUT, "Auto-stop after %u sec", g_runTimeoutSec);
      broadcastStatus();
    }
  }

  // ── Emergency Stop: ISR flag + hardware poll ──
  if (g_emergencyPending) {
    g_emergencyPending = false;
    emergencyStop();
    // Re-attach interrupt with 50ms debounce
    static uint32_t lastEmergReattach = 0;
    if (millis() - lastEmergReattach > 50) {
      attachInterrupt(digitalPinToInterrupt(EMERGENCY_PIN), onEmergencyISR, FALLING);
      lastEmergReattach = millis();
    }
  }
  // Hardware polling: backup for edge-triggered misses (debounced)
  {
    static uint32_t lastPollEmerg = 0;
    if (digitalRead(EMERGENCY_PIN) == LOW && millis() - lastPollEmerg > 200) {
      lastPollEmerg = millis();
      emergencyStop();
    }
  }

  // ── Soft-start completion (deferred from task context) ──
  if (g_softStartComplete) {
    g_softStartComplete = false;
    float target;
    portENTER_CRITICAL(&s_mux);
    target = g_duty;
    portEXIT_CRITICAL(&s_mux);
    evtLog.log(EVT_START, "Soft-start complete, running at %.1f%%", target);
    broadcastStatus();
  }

  // ── Periodic WS broadcast (every ~1s) ──
  static uint32_t lastBroadcast = 0;
  if (millis() - lastBroadcast >= 1000) {
    lastBroadcast = millis();
    broadcastStatus();
  }

  // ── Clean up disconnected WS clients ──
  ws.cleanupClients();

  vTaskDelay(pdMS_TO_TICKS(50));
}
