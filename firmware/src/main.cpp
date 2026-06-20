/*
 * IH-2000 Half-Bridge Firmware
 * =============================
 * WiFiProvisioner  → captive portal WiFi setup
 * MCPWM            → complementary PWM   GPIO18 (high) / GPIO19 (low)
 *                    frequency 10-300 kHz, dead time 0-5000 ns, duty 50% fixed
 * PCNT freq meter  → optional: read PWM on GPIO34
 * Web dashboard    → start/stop, freq, dead time, freq meter
 */

#include <Arduino.h>
#include <WiFiProvisioner.h>
#include <WebServer.h>
#include <Preferences.h>
#include "MCPWMController.h"

/* ─── Freq meter (PCNT) — optional ──────────────────────── */
#define PIN_FREQ_IN   GPIO_NUM_34   /* jumper from GPIO18/19 */
#define MEASURE_MS    500
#include "driver/pcnt.h"
static uint32_t s_freqHz = 0;

/* ─── Globals ────────────────────────────────────────────── */
MCPWMController pwm;
WebServer       webSrv(80);
Preferences     prefs;

/* ═══════════════════════════════════════════════════════════
 *  Freq meter (PCNT) — optional, reads GPIO34
 * ═══════════════════════════════════════════════════════════ */
void initFreqMeter() {
    pcnt_config_t cfg{};
    cfg.pulse_gpio_num = PIN_FREQ_IN;
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.unit           = PCNT_UNIT_0;
    cfg.pos_mode       = PCNT_COUNT_INC;
    cfg.neg_mode       = PCNT_COUNT_DIS;
    cfg.lctrl_mode     = PCNT_MODE_KEEP;
    cfg.hctrl_mode     = PCNT_MODE_KEEP;
    cfg.counter_h_lim  = 32767;
    cfg.counter_l_lim  = 0;
    pcnt_unit_config(&cfg);
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
    Serial.printf("[FREQ] Meter on GPIO%d\n", PIN_FREQ_IN);
}

void freqTask(void*) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(MEASURE_MS));
        int16_t cnt;
        pcnt_get_counter_value(PCNT_UNIT_0, &cnt);
        pcnt_counter_clear(PCNT_UNIT_0);
        s_freqHz = (uint32_t)cnt * (1000u / MEASURE_MS);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  Web interface (PROGMEM)
 * ═══════════════════════════════════════════════════════════ */
static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset=UTF-8>
<meta name=viewport content='width=device-width,initial-scale=1,maximum-scale=1'>
<title>IH-2000</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,sans-serif;padding:20px;background:#0d0d0d;color:#0f0;max-width:500px;margin:0 auto}
h1{text-align:center;font-size:20px;letter-spacing:2px;margin-bottom:20px;color:#0f0}
.card{border:1px solid #222;border-radius:12px;padding:20px;margin-bottom:12px}
.row{display:flex;justify-content:space-between;align-items:center;padding:6px 0}
.lbl{color:#666;font-size:13px}
.val{font-size:18px;font-weight:bold;font-variant-numeric:tabular-nums}
.val.green{color:#0f0}
.val.red{color:#f00}
input[type=range]{width:100%;margin:8px 0;accent-color:#0f0;background:transparent}
input[type=range]::-webkit-slider-runnable-track{height:4px;background:#222;border-radius:2px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;height:20px;width:20px;border-radius:50%;background:#0f0;margin-top:-8px}
.slider-label{display:flex;justify-content:space-between;font-size:12px;color:#555}
.btns{display:flex;gap:12px;margin-top:12px}
.btn{flex:1;padding:12px;border:0;border-radius:8px;font-size:16px;font-weight:600;cursor:pointer;text-transform:uppercase;letter-spacing:1px}
.btn-start{background:#0f0;color:#000}
.btn-stop{background:#f00;color:#fff}
.btn-start:disabled,.btn-stop:disabled{opacity:0.3;cursor:not-allowed}
.wifi{font-size:11px;color:#444;text-align:center;margin-top:16px}
</style>
</head>
<body>
<h1>IH‑2000</h1>

<div class=card>
<div class=row><span class=lbl>Status</span><span class='val' id=status>—</span></div>
</div>

<div class=card>
<div class=btns>
<button class='btn btn-start' id=btnStart onclick='doStart()'>Start</button>
<button class='btn btn-stop' id=btnStop onclick='doStop()'>Stop</button>
</div>
</div>

<div class=card>
<div class=row><span class=lbl>Frequency</span><span class=val id=freqVal>—</span></div>
<input type=range id=freq min=10000 max=300000 value=50000 oninput='setFreq(this.value)'>
<div class=slider-label><span>10 kHz</span><span>300 kHz</span></div>
<div class=row style=margin-top:12px><span class=lbl>Dead Time</span><span class=val id=dtVal>—</span></div>
<input type=range id=dt min=0 max=5000 value=500 oninput='setDT(this.value)'>
<div class=slider-label><span>0 ns</span><span>5000 ns</span></div>
</div>

<div class=card>
<div class=row><span class=lbl>Input Freq</span><span class=val id=inFreq>—</span></div>
<div class=wifi id=wifiInfo></div>
</div>

<script>
let lastFreq=50000, lastDT=500, cfgTimer=null;

function cfg(){clearTimeout(cfgTimer)}
function doCfg(){fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({freq:lastFreq,dt:lastDT})})}
function setFreq(v){lastFreq=+v;document.getElementById('freqVal').textContent=v+' Hz';cfg();cfgTimer=setTimeout(doCfg,200)}
function setDT(v){lastDT=+v;document.getElementById('dtVal').textContent=v+' ns';cfg();cfgTimer=setTimeout(doCfg,200)}

function doStart(){fetch('/api/start',{method:'POST'})}
function doStop(){fetch('/api/stop',{method:'POST'})}

async function poll(){
 try{
  const r=await fetch('/api/status');if(!r.ok)return;
  const d=await r.json();
  const run=d.running;
  document.getElementById('status').textContent=run?'RUNNING':'STOPPED';
  document.getElementById('status').className='val '+(run?'green':'red');
  document.getElementById('btnStart').disabled=run;
  document.getElementById('btnStop').disabled=!run;
  if(d.freq){document.getElementById('freqVal').textContent=d.freq+' Hz';document.getElementById('freq').value=d.freq}
  if(d.dt!=null){document.getElementById('dtVal').textContent=d.dt+' ns';document.getElementById('dt').value=d.dt}
  if(d.inFreq!=null)document.getElementById('inFreq').textContent=d.inFreq+' Hz';
  document.getElementById('wifiInfo').textContent='IP: '+d.ip+'  RSSI: '+d.rssi+' dBm'+'  SSID: '+d.ssid;
 }catch(e){}
}
setInterval(poll,500);poll();
</script>
</body>
</html>
)rawliteral";

/* ═══════════════════════════════════════════════════════════
 *  Web handlers
 * ═══════════════════════════════════════════════════════════ */
#include <ArduinoJson.h>

void handleRoot() {
    webSrv.send_P(200, "text/html", PAGE_HTML);
}

void handleStatus() {
    JsonDocument doc;
    doc["running"] = pwm.isRunning();
    doc["freq"]    = pwm.getFrequency();
    doc["dt"]      = pwm.getDeadTime();
    doc["inFreq"]  = s_freqHz;
    doc["ip"]      = WiFi.localIP().toString();
    doc["rssi"]    = WiFi.RSSI();
    doc["ssid"]    = WiFi.SSID();

    String json;
    serializeJson(doc, json);
    webSrv.send(200, "application/json", json);
}

void handleConfig() {
    if (!webSrv.hasArg("plain")) { webSrv.send(400, "text/plain", "no body"); return; }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, webSrv.arg("plain"));
    if (err) { webSrv.send(400, "text/plain", "bad json"); return; }

    if (doc.containsKey("freq")) pwm.setFrequency(doc["freq"].as<float>());
    if (doc.containsKey("dt"))   pwm.setDeadTime(doc["dt"].as<float>());

    handleStatus();
}

void handleStart() {
    pwm.start();
    webSrv.send(200, "application/json", "{\"ok\":true}");
}

void handleStop() {
    pwm.stop();
    webSrv.send(200, "application/json", "{\"ok\":true}");
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

/* ─── SVG logo for captive portal ─── */
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
    Serial.println(F("  IH-2000  HALF-BRIDGE"));
    Serial.println(F("═══════════════════════════════"));

    /* ── 1. MCPWM half-bridge ── */
    Serial.printf("[PWM] Init  GPIO18=HIGH  GPIO19=LOW\n");
    esp_err_t err = pwm.begin(50000.0f, 500.0f);
    if (err != ESP_OK) {
        Serial.printf("[PWM] FAIL: %d\n", err);
    } else {
        Serial.printf("[PWM] OK  50 kHz  DT=500 ns\n");
    }

    /* ── 2. Freq meter (optional) ── */
    initFreqMeter();
    xTaskCreatePinnedToCore(freqTask, "freqMeter", 2048,
                            nullptr, 5, nullptr, 1);

    /* ── 3. WiFi ── */
    String savedSSID, savedPass;
    if (loadCreds(savedSSID, savedPass)) {
        Serial.printf("[WiFi] Saved: %s\n", savedSSID.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000)
            delay(50);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[WiFi] Starting captive portal..."));

        WiFiProvisioner provisioner;
        provisioner.getConfig().AP_NAME       = "IH-2000";
        provisioner.getConfig().HTML_TITLE    = "IH-2000 Configuration";
        provisioner.getConfig().THEME_COLOR   = "#e53935";
        provisioner.getConfig().SVG_LOGO      = IH_LOGO;
        provisioner.getConfig().PROJECT_TITLE = "IH-2000 Induction Heater";
        provisioner.getConfig().PROJECT_SUB_TITLE = "Half-Bridge Controller";
        provisioner.getConfig().PROJECT_INFO  =
            "Connect to your WiFi to access the PWM control dashboard.";
        provisioner.getConfig().FOOTER_TEXT   = "IH-2000 Half-Bridge";
        provisioner.getConfig().SHOW_INPUT_FIELD   = false;
        provisioner.getConfig().SHOW_RESET_FIELD   = true;
        provisioner.getConfig().RESET_CONFIRMATION_TEXT = "Clear WiFi settings.";
        provisioner.getConfig().CONNECTION_SUCCESSFUL = "Connected! Dashboard ready.";

        provisioner.onSuccess([](const char *s, const char *p, const char *) {
            saveCreds(s, p);
        });
        provisioner.onFactoryReset([]() { clearCreds(); });

        if (!provisioner.startProvisioning()) {
            Serial.println(F("[FATAL] Provisioning failed — restart"));
            delay(2000);
            ESP.restart();
        }
        WiFi.setAutoReconnect(true);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println(F("[FATAL] No WiFi — restart"));
        delay(2000);
        ESP.restart();
    }

    Serial.printf("[WiFi] %s  IP=%s  RSSI=%d\n",
                  WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());

    /* ── 4. Web server ── */
    webSrv.on("/",           HTTP_GET,  handleRoot);
    webSrv.on("/api/status", HTTP_GET,  handleStatus);
    webSrv.on("/api/config", HTTP_POST, handleConfig);
    webSrv.on("/api/start",  HTTP_POST, handleStart);
    webSrv.on("/api/stop",   HTTP_POST, handleStop);
    webSrv.begin();
    Serial.printf("[WEB] http://%s/\n", WiFi.localIP().toString().c_str());

    Serial.println(F("\n══════ READY ══════"));
}

/* ═══════════════════════════════════════════════════════════
 *  loop()
 * ═══════════════════════════════════════════════════════════ */
void loop() {
    webSrv.handleClient();
}
