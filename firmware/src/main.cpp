/*
 * IH-2000 Test Firmware
 * ======================
 * WiFiProvisioner  → captive portal WiFi setup
 * LEDC PWM         → 50 kHz fixed on GPIO22
 * PCNT freq meter  → hardware pulse counter on GPIO34
 * Web dashboard    → live frequency display
 *
 * TEST: jumper GPIO22 → GPIO34, read frequency on web UI.
 */

#include <Arduino.h>
#include <WiFiProvisioner.h>
#include <WebServer.h>
#include <Preferences.h>
#include "driver/pcnt.h"
#include "driver/ledc.h"

/* ─── Pinout ─────────────────────────────────────────────── */
#define PIN_PWM_OUT   GPIO_NUM_22
#define PIN_FREQ_IN   GPIO_NUM_34

/* ─── PWM (LEDC) ────────────────────────────────────────── */
#define PWM_FREQ_HZ   50000u
#define PWM_DUTY_PCT  50
#define LEDC_TMR      LEDC_TIMER_0
#define LEDC_CH       LEDC_CHANNEL_0

/* ─── Freq meter (PCNT) ─────────────────────────────────── */
#define PCNT_UNIT     PCNT_UNIT_0
#define MEASURE_MS    500

/* ─── Globals ────────────────────────────────────────────── */
WebServer   webSrv(80);
Preferences prefs;
uint32_t    g_freqHz = 0;

/* ═══════════════════════════════════════════════════════════
 *  PWM init — fixed 50 kHz, 50% duty on GPIO22
 * ═══════════════════════════════════════════════════════════ */
void initPWM() {
    ledc_timer_config_t timer{};
    timer.speed_mode      = LEDC_HIGH_SPEED_MODE;
    timer.timer_num       = LEDC_TMR;
    timer.freq_hz         = PWM_FREQ_HZ;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    ledc_timer_config(&timer);

    ledc_channel_config_t chan{};
    chan.gpio_num   = PIN_PWM_OUT;
    chan.speed_mode = LEDC_HIGH_SPEED_MODE;
    chan.channel    = LEDC_CH;
    chan.timer_sel  = LEDC_TMR;
    chan.duty       = (1023u * PWM_DUTY_PCT) / 100;
    ledc_channel_config(&chan);

    Serial.printf("[PWM] GPIO%d = %u Hz  %u%%\n",
                  PIN_PWM_OUT, PWM_FREQ_HZ, PWM_DUTY_PCT);
}

/* ═══════════════════════════════════════════════════════════
 *  Freq meter init — PCNT counts rising edges
 * ═══════════════════════════════════════════════════════════ */
void initFreqMeter() {
    pcnt_config_t cfg{};
    cfg.pulse_gpio_num = PIN_FREQ_IN;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.unit           = PCNT_UNIT;
    cfg.pos_mode       = PCNT_COUNT_INC;
    cfg.neg_mode       = PCNT_COUNT_DIS;
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;
    cfg.counter_h_lim  = 32767;
    cfg.counter_l_lim  = 0;

    pcnt_unit_config(&cfg);
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);

    Serial.printf("[FREQ] GPIO%d rising-edge count\n", PIN_FREQ_IN);
}

/* ─── Freq measurement task (core 1) ─── */
void freqTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(MEASURE_MS));
        int16_t cnt;
        pcnt_get_counter_value(PCNT_UNIT, &cnt);
        pcnt_counter_clear(PCNT_UNIT);
        g_freqHz = (uint32_t)cnt * (1000u / MEASURE_MS);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Web handlers
 * ═══════════════════════════════════════════════════════════ */
void handleRoot() {
    String html;
    html.reserve(800);
    html  = F("<!DOCTYPE html><html><head>"
              "<meta charset=UTF-8>"
              "<meta name=viewport content='width=device-width,initial-scale=1'>"
              "<title>IH-2000</title>"
              "<style>"
              "*{box-sizing:border-box;margin:0;padding:0}"
              "body{font-family:monospace;padding:20px;background:#111;color:#0f0}"
              "h1{text-align:center;font-size:22px;margin-bottom:24px;letter-spacing:2px}"
              ".card{border:1px solid #333;border-radius:12px;padding:32px 24px;"
              "text-align:center;max-width:400px;margin:0 auto}"
              ".freq{font-size:64px;font-weight:bold;font-variant-numeric:tabular-nums}"
              ".unit{font-size:16px;color:#888;margin-top:4px}"
              ".info{color:#555;font-size:13px;margin-top:20px;word-break:break-all}"
              ".label{color:#555;font-size:12px;margin-bottom:8px;"
              "text-transform:uppercase;letter-spacing:1px}"
              "</style></head><body>"
              "<h1>IH‑2000</h1>"
              "<div class=card>"
              "<div class=label>Frequency</div>"
              "<div class=freq id=f>0</div>"
              "<div class=unit>Hz</div>"
              "<div class=info id=i>&mdash;</div>"
              "</div>"
              "<script>"
              "function u(){"
              "fetch('/freq').then(r=>r.text()).then(v=>document.getElementById('f').textContent=v)"
              ".catch(()=>{});"
              "fetch('/info').then(r=>r.json()).then(d=>"
              "document.getElementById('i').textContent='IP: '+d.ip+'  RSSI: '+d.rssi+'dBm')"
              ".catch(()=>{})}"
              "setInterval(u,500);u();"
              "</script></body></html>");
    webSrv.send(200, "text/html", html);
}

void handleFreq() {
    webSrv.send(200, "text/plain", String(g_freqHz));
}

void handleInfo() {
    String json = "{\"ip\":\"";
    json += WiFi.localIP().toString();
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI());
    json += ",\"ssid\":\"";
    json += WiFi.SSID();
    json += "\"}";
    webSrv.send(200, "application/json", json);
}

void initWeb() {
    webSrv.on("/",     HTTP_GET, handleRoot);
    webSrv.on("/freq", HTTP_GET, handleFreq);
    webSrv.on("/info", HTTP_GET, handleInfo);
    webSrv.begin();
    Serial.printf("[WEB] http://%s/\n", WiFi.localIP().toString().c_str());
}

/* ═══════════════════════════════════════════════════════════
 *  WiFi credential helpers
 * ═══════════════════════════════════════════════════════════ */
bool loadCreds(String &ssid, String &pass) {
    prefs.begin("ih-wifi", true);
    ssid = prefs.getString("ssid", "");
    pass = prefs.getString("pass", "");
    prefs.end();
    return ssid.length() > 0;
}

void saveCreds(const char *ssid, const char *pass) {
    prefs.begin("ih-wifi", false);
    prefs.putString("ssid", ssid ? ssid : "");
    prefs.putString("pass", pass ? pass : "");
    prefs.end();
    Serial.printf("[SAVE] %s\n", ssid);
}

void clearCreds() {
    prefs.begin("ih-wifi", false);
    prefs.clear();
    prefs.end();
    Serial.println(F("[SAVE] Credentials cleared"));
}

/* ═══════════════════════════════════════════════════════════
 *  SVG logo for provisioning page
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
        provisioner.getConfig().CONNECTION_SUCCESSFUL =
            "Your IH-2000 is now connected! The dashboard is ready.";

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

    Serial.printf("[WiFi] SSID='%s'  IP=%s  RSSI=%d\n",
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
    webSrv.handleClient();
}
