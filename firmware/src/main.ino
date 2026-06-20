/*
 * IH-2000 Test Firmware
 * ======================
 * WiFiProvisioner  → captive portal WiFi setup
 * LEDC PWM         → 50 kHz fixed on GPIO22 (no external lib)
 * PCNT freq meter  → hardware pulse counter on GPIO34
 * Web dashboard    → live frequency display
 *
 * TEST: connect GPIO22 → GPIO34 with a jumper, read frequency on web UI.
 */

#include <Arduino.h>
#include <WiFiProvisioner.h>
#include <WebServer.h>
#include <Preferences.h>
#include "driver/pcnt.h"
#include "driver/ledc.h"

/* ─── Pinout ─────────────────────────────────────────────── */
#define PIN_PWM_OUT   GPIO_NUM_22    /* PWM output (fixed)    */
#define PIN_FREQ_IN   GPIO_NUM_34    /* Freq meter input      */

/* ─── PWM (LEDC, built-in, zero external deps) ──────────── */
#define PWM_FREQ_HZ   50000u         /* 50 kHz                */
#define PWM_DUTY_PCT  50             /* 50 %                  */
#define LEDC_TIMER    LEDC_TIMER_0
#define LEDC_CHAN     LEDC_CHANNEL_0

/* ─── Freq meter (PCNT, hardware counter) ───────────────── */
#define PCNT_UNIT     PCNT_UNIT_0
#define MEASURE_MS    500            /* read interval (ms)    */

/* ─── Globals ────────────────────────────────────────────── */
static WebServer    s_web(80);
static Preferences  s_prefs;
static uint32_t     s_freqHz = 0;    /* updated by freqTask  */

/* ═══════════════════════════════════════════════════════════
 *  PWM — fixed 50 kHz, 50 % duty on GPIO22
 * ═══════════════════════════════════════════════════════════ */
static void initPWM() {
    ledc_timer_config_t timer = {};
    timer.speed_mode      = LEDC_HIGH_SPEED_MODE;
    timer.timer_num       = LEDC_TIMER;
    timer.freq_hz         = PWM_FREQ_HZ;
    timer.duty_resolution = LEDC_TIMER_10_BIT;       /* 0..1023 */
    ledc_timer_config(&timer);

    ledc_channel_config_t chan = {};
    chan.gpio_num   = PIN_PWM_OUT;
    chan.speed_mode = LEDC_HIGH_SPEED_MODE;
    chan.channel    = LEDC_CHAN;
    chan.timer_sel  = LEDC_TIMER;
    chan.duty       = (1023u * PWM_DUTY_PCT) / 100;  /* 50 % */
    ledc_channel_config(&chan);

    Serial.printf("[PWM] GPIO%d = %u Hz  %u%%\n",
                  PIN_PWM_OUT, PWM_FREQ_HZ, PWM_DUTY_PCT);
}

/* ═══════════════════════════════════════════════════════════
 *  Freq meter — PCNT counts rising edges on GPIO34
 * ═══════════════════════════════════════════════════════════ */
static void initFreqMeter() {
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = PIN_FREQ_IN;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.unit           = PCNT_UNIT;
    cfg.pos_mode       = PCNT_COUNT_INC;    /* rising edge     */
    cfg.neg_mode       = PCNT_COUNT_DIS;    /* ignore falling  */
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;
    cfg.counter_h_lim  = 32767;
    cfg.counter_l_lim  = 0;

    pcnt_unit_config(&cfg);
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);

    Serial.printf("[FREQ] GPIO%d ← rising edges\n", PIN_FREQ_IN);
}

/* ─── Freq task (core 1, 500 ms interval) ─── */
static void freqTask(void*) {
    TickType_t last = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(MEASURE_MS));
        int16_t cnt;
        pcnt_get_counter_value(PCNT_UNIT, &cnt);
        pcnt_counter_clear(PCNT_UNIT);
        s_freqHz = (uint32_t)cnt * (1000u / MEASURE_MS);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Web handlers
 * ═══════════════════════════════════════════════════════════ */
static void handleRoot() {
    String html = FPSTR(R"(
<!DOCTYPE html>
<html>
<head>
<meta charset=UTF-8>
<meta name=viewport content='width=device-width,initial-scale=1'>
<title>IH-2000</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:monospace;padding:20px;background:#111;color:#0f0}
h1{text-align:center;font-size:22px;margin-bottom:24px;letter-spacing:2px}
.card{border:1px solid #333;border-radius:12px;padding:32px 24px;text-align:center;max-width:400px;margin:0 auto}
.freq{font-size:64px;font-weight:bold;font-variant-numeric:tabular-nums}
.unit{font-size:16px;color:#888;margin-top:4px}
.info{color:#555;font-size:13px;margin-top:20px;word-break:break-all}
.label{color:#555;font-size:12px;margin-bottom:8px;text-transform:uppercase;letter-spacing:1px}
</style>
</head>
<body>
<h1>IH‑2000</h1>
<div class=card>
<div class=label>Frequency</div>
<div class=freq id=f>0</div>
<div class=unit>Hz</div>
<div class=info id=i>—</div>
</div>
<script>
function u(){fetch('/freq').then(r=>r.text()).then(v=>document.getElementById('f').textContent=v).catch(()=>{});fetch('/info').then(r=>r.json()).then(d=>document.getElementById('i').textContent='IP: '+d.ip+'  RSSI: '+d.rssi+'dBm').catch(()=>{})}
setInterval(u,500);u();
</script>
</body>
</html>
    )");
    s_web.send(200, "text/html", html);
}

static void handleFreq() {
    s_web.send(200, "text/plain", String(s_freqHz));
}

static void handleInfo() {
    String json = "{";
    json += "\"ip\":\""   + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":"   + String(WiFi.RSSI()) + ",";
    json += "\"ssid\":\"" + String(WiFi.SSID()) + "\"";
    json += "}";
    s_web.send(200, "application/json", json);
}

static void initWeb() {
    s_web.on("/",     HTTP_GET, handleRoot);
    s_web.on("/freq", HTTP_GET, handleFreq);
    s_web.on("/info", HTTP_GET, handleInfo);
    s_web.begin();
    Serial.printf("[WEB] http://%s/\n", WiFi.localIP().toString().c_str());
}

/* ═══════════════════════════════════════════════════════════
 *  WiFi credential helpers  (Preferences namespace "ih-wifi")
 * ═══════════════════════════════════════════════════════════ */
static bool loadCreds(String &ssid, String &pass) {
    s_prefs.begin("ih-wifi", true);
    ssid = s_prefs.getString("ssid", "");
    pass = s_prefs.getString("pass", "");
    s_prefs.end();
    return ssid.length() > 0;
}

static void saveCreds(const char *ssid, const char *pass) {
    s_prefs.begin("ih-wifi", false);
    s_prefs.putString("ssid", ssid ? ssid : "");
    s_prefs.putString("pass", pass ? pass : "");
    s_prefs.end();
    Serial.printf("[SAVE] %s\n", ssid);
}

static void clearCreds() {
    s_prefs.begin("ih-wifi", false);
    s_prefs.clear();
    s_prefs.end();
    Serial.println("[SAVE] Credentials cleared");
}

/* ═══════════════════════════════════════════════════════════
 *  Provisioning SVG logo (compact)
 * ═══════════════════════════════════════════════════════════ */
static const char IH_LOGO[] PROGMEM = R"rawliteral(
<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 32 32">
  <circle cx="16" cy="16" r="14" fill="none" stroke="var(--theme-color)" stroke-width="2"/>
  <text x="16" y="21" text-anchor="middle" font-size="14" font-weight="bold" fill="var(--font-color)">IH</text>
</svg>
)rawliteral";

/* ═══════════════════════════════════════════════════════════
 *  setup()
 * ═══════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("═══════════════════════════════"));
    Serial.println(F("  IH-2000  TEST  FIRMWARE"));
    Serial.println(F("  WiFiProvisioner + LEDC + PCNT"));
    Serial.println(F("═══════════════════════════════"));

    /* ── 1. PWM on GPIO22 ── */
    initPWM();

    /* ── 2. Freq meter on GPIO34 ── */
    initFreqMeter();

    /* ── 3. Freq task (core 1) ── */
    xTaskCreatePinnedToCore(freqTask, "freqMeter", 2048,
                            nullptr, 5, nullptr, 1);

    /* ── 4. WiFi ── */
    String savedSSID, savedPass;
    if (loadCreds(savedSSID, savedPass)) {
        Serial.printf("[WiFi] Saved credentials for '%s'\n",
                       savedSSID.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());

        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - t0 < 10000) {
            delay(50);
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[WiFi] No saved credentials — starting captive portal\n"));

        WiFiProvisioner provisioner;

        /* Customise the provisioning UI */
        provisioner.getConfig().AP_NAME       = "IH-TEST";
        provisioner.getConfig().HTML_TITLE    = "IH-2000 Configuration";
        provisioner.getConfig().THEME_COLOR   = "#e53935";
        provisioner.getConfig().SVG_LOGO      = IH_LOGO;
        provisioner.getConfig().PROJECT_TITLE = "IH-2000 Induction Heater";
        provisioner.getConfig().PROJECT_SUB_TITLE = "Test Firmware";
        provisioner.getConfig().PROJECT_INFO  =
            "Connect to your WiFi network to access the frequency meter dashboard.";
        provisioner.getConfig().FOOTER_TEXT   = "IH-2000 Test Firmware";
        provisioner.getConfig().SHOW_INPUT_FIELD   = false;
        provisioner.getConfig().SHOW_RESET_FIELD   = true;
        provisioner.getConfig().RESET_CONFIRMATION_TEXT =
            "This will clear saved WiFi credentials.";

        provisioner.onSuccess([](const char *ssid,
                                  const char *pass,
                                  const char *) {
            saveCreds(ssid, pass);
        });

        provisioner.onFactoryReset([]() {
            clearCreds();
        });

        Serial.println(F("[WiFi] AP 'IH-TEST' — connect & configure\n"));

        if (!provisioner.startProvisioning()) {
            Serial.println(F("[FATAL] Provisioning failed — restarting"));
            delay(2000);
            ESP.restart();
        }

        WiFi.setAutoReconnect(true);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[FATAL] WiFi not connected — restarting"));
        delay(2000);
        ESP.restart();
    }

    Serial.printf("[WiFi] Connected  SSID='%s'  IP=%s  RSSI=%d dBm\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());

    /* ── 5. Web server ── */
    initWeb();

    Serial.println(F("\n══════ READY ══════"));
}

/* ═══════════════════════════════════════════════════════════
 *  loop()
 * ═══════════════════════════════════════════════════════════ */
void loop() {
    s_web.handleClient();
}
