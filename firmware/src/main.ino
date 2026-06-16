#include "provisioning/WiFiProvisioning.h"
#include "hal/PWMManager.h"
#include "hal/ConfigStore.h"
#include "core/Types.h"
#include "core/ParamValidator.h"

WiFiProvisioning wifi;
PWMManager pwm(PWM_PIN_A, PWM_PIN_B);
ConfigStore config;
CoreParams params;
WebServer* apiServer = nullptr;
bool restartPending = false;

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>IH</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:monospace;padding:20px;background:#111;color:#0f0}
h1{font-size:20px;margin-bottom:16px}
.s{margin-bottom:20px;padding:12px;border:1px solid #333}
.r{display:flex;justify-content:space-between;padding:4px 0}
.l{color:#888}
.v{color:#0f0}
button{padding:8px 24px;font-size:16px;cursor:pointer;border:0;border-radius:4px}
.bs{background:#0a0;color:#fff}
.bp{background:#a00;color:#fff}
button:disabled{opacity:.4}
input{background:#000;border:1px solid #333;color:#0f0;padding:6px;width:80px;text-align:center;font-family:monospace}
.fr{display:flex;gap:8px;align-items:center;margin:4px 0}
.fr lb{width:100px;color:#888}
.ba{background:#00a;color:#fff;padding:6px 16px;border:0;border-radius:4px;cursor:pointer}
.t{padding:2px 8px;border-radius:3px;font-weight:bold}
.ti{background:#333;color:#888}
.ts{background:#aa0;color:#000}
.tr{background:#0a0;color:#fff}
.to{background:#a00;color:#fff}
</style>
</head>
<body>
<h1>IH Control</h1>
<div class="s" id="st">
<div class="r"><span class="l">Status</span><span id="s" class="v">--</span></div>
<div class="r"><span class="l">Freq</span><span id="f" class="v">-- Hz</span></div>
<div class="r"><span class="l">Duty</span><span id="d" class="v">-- %</span></div>
<div class="r"><span class="l">Dead Time</span><span id="dt" class="v">-- ns</span></div>
<div class="r"><span class="l">Soft Start</span><span id="ss" class="v">-- ms</span></div>
</div>
<div class="s">
<div class="r"><span class="l">IP</span><span id="ip" class="v">--</span></div>
<div class="r"><span class="l">RSSI</span><span id="rssi" class="v">-- dBm</span></div>
</div>
<div class="s" style="text-align:center">
<button id="b1" class="bs" onclick="go(1)">Start</button>
<button id="b2" class="bp" onclick="go(0)">Stop</button>
</div>
<div class="s">
<h2>Params</h2>
<div class="fr"><lb>Freq (Hz)</lb><input id="i0" type="number" min="10000" max="30000"><button class="ba" onclick="ap()">Set</button></div>
<div class="fr"><lb>Duty (%)</lb><input id="i1" type="number" min="5" max="95" step="0.1"></div>
<div class="fr"><lb>DT (ns)</lb><input id="i2" type="number" min="100" max="5000"></div>
<div class="fr"><lb>SS (ms)</lb><input id="i3" type="number" min="500" max="10000"></div>
</div>
<script>
function $(i){return document.getElementById(i)}
function tag(s){
 if(s=='IDLE') return '<b class="t ti">IDLE</b>';
 if(s=='SOFT_START') return '<b class="t ts">SOFT START</b>';
 if(s=='RUNNING') return '<b class="t tr">RUNNING</b>';
 if(s=='STOPPING') return '<b class="t to">STOPPING</b>';
 return s;
}
function poll(){
 fetch('/status').then(r=>r.json()).then(d=>{
  $('s').innerHTML=tag(d.state);
  $('f').textContent=d.freq+' Hz';
  $('d').textContent=d.duty+'%';
  $('dt').textContent=d.dt+' ns';
  $('ss').textContent=d.ss+' ms';
  $('ip').textContent=d.ip;
  $('rssi').textContent=d.rssi+' dBm';
  $('b1').disabled=d.running;
  $('b2').disabled=!d.running;
  $('i0').value=d.freq;$('i1').value=d.duty;$('i2').value=d.dt;$('i3').value=d.ss;
 }).catch(function(){});
}
function go(v){fetch(v?'/start':'/stop').then(r=>r.json()).then(function(d){if(d.ok)poll()});}
function ap(){fetch('/set?f='+$('i0').value+'&d='+$('i1').value+'&dt='+$('i2').value+'&ss='+$('i3').value).then(r=>r.json()).then(function(){poll()});}
setInterval(poll,2500);poll();
</script>
</body>
</html>
)rawliteral";

void startAPIServer();
void stopAPIServer();

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=== IH v5 ==="));

    config.begin();
    params = ParamValidator::clamp(config.load());
    pwm.begin(params);

    Serial.printf("[Main] ID: %s\n",
        String((uint32_t)(ESP.getEfuseMac() >> 24), HEX).c_str());

    wifi.begin();
}

void loop() {
    if (restartPending) {
        delay(500);
        ESP.restart();
    }

    wifi.handle();

    if (wifi.isProvisioned() && apiServer == nullptr) {
        startAPIServer();
    } else if (!wifi.isProvisioned() && apiServer != nullptr) {
        stopAPIServer();
    }

    if (apiServer) {
        apiServer->handleClient();
    }

    pwm.softStartTick();
}

void startAPIServer() {
    apiServer = new WebServer(80);

    apiServer->on("/status", HTTP_GET, []() {
        String json;
        json.reserve(256);
        String stateStr;
        switch (pwm.getState()) {
            case RunState::IDLE:       stateStr = "IDLE"; break;
            case RunState::SOFT_START: stateStr = "SOFT_START"; break;
            case RunState::RUNNING:    stateStr = "RUNNING"; break;
            case RunState::STOPPING:   stateStr = "STOPPING"; break;
        }
        json += "{\"running\":";   json += pwm.isRunning() ? "true" : "false";
        json += ",\"softStart\":"; json += pwm.isSoftStarting() ? "true" : "false";
        json += ",\"state\":\"";   json += stateStr;
        json += "\",\"freq\":";    json += String(params.freq, 0);
        json += ",\"duty\":";      json += String(pwm.getDuty(), 1);
        json += ",\"dt\":";        json += String(params.deadTimeNs, 0);
        json += ",\"ss\":";        json += String(params.softStartMs);
        json += ",\"dutyTarget\":"; json += String(params.duty, 1);
        json += ",\"ip\":\"";      json += wifi.getIP().toString();
        json += "\",\"rssi\":";    json += String(WiFi.RSSI());
        json += "}";
        apiServer->send(200, "application/json", json);
    });

    apiServer->on("/", HTTP_GET, []() {
        apiServer->send_P(200, "text/html", DASHBOARD_HTML);
    });

    apiServer->on("/start", HTTP_GET, []() {
        if (pwm.isRunning()) {
            apiServer->send(200, "application/json", "{\"ok\":false,\"msg\":\"already running\"}");
            return;
        }
        pwm.start(params.duty, params.softStartMs);
        apiServer->send(200, "application/json", "{\"ok\":true}");
    });

    apiServer->on("/stop", HTTP_GET, []() {
        pwm.stop();
        apiServer->send(200, "application/json", "{\"ok\":true}");
    });

    apiServer->on("/set", HTTP_GET, []() {
        CoreParams p = params;

        if (apiServer->hasArg("f"))
            p.freq = apiServer->arg("f").toFloat();
        if (apiServer->hasArg("d"))
            p.duty = apiServer->arg("d").toFloat();
        if (apiServer->hasArg("dt"))
            p.deadTimeNs = apiServer->arg("dt").toFloat();
        if (apiServer->hasArg("ss"))
            p.softStartMs = apiServer->arg("ss").toInt();

        p = ParamValidator::clamp(p);
        params = p;
        config.save(params);

        pwm.setFrequency(params.freq);
        pwm.setDeadTime(params.deadTimeNs);
        if (pwm.isRunning())
            pwm.setDuty(params.duty);

        String json = config.toJSON(params);
        apiServer->send(200, "application/json", json);
    });

    apiServer->on("/wifi-status", HTTP_GET, []() {
        String json;
        json.reserve(128);
        json += "{\"state\":\"";
        json += wifi.isProvisioned() ? "connected" : "disconnected";
        json += "\",\"ip\":\"";   json += wifi.getIP().toString();
        json += "\",\"rssi\":";   json += String(WiFi.RSSI());
        json += "}";
        apiServer->send(200, "application/json", json);
    });

    apiServer->on("/scan", HTTP_GET, []() {
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true);
            apiServer->send(200, "application/json", "{\"scanning\":true}");
        } else if (n == -1) {
            apiServer->send(200, "application/json", "{\"scanning\":true}");
        } else {
            String json = "[";
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                String ssid = WiFi.SSID(i);
                if (ssid.length() == 0) continue;
                json += "{\"ssid\":\"";
                json += ssid;
                json += "\",\"rssi\":";
                json += String(WiFi.RSSI(i));
                json += ",\"secure\":";
                json += WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false";
                json += "}";
            }
            json += "]";
            WiFi.scanDelete();
            apiServer->send(200, "application/json", json);
        }
    });

    apiServer->on("/connect", HTTP_POST, []() {
        String ssid = apiServer->arg("ssid");
        String pass = apiServer->arg("pass");
        if (ssid.length() == 0) {
            apiServer->send(400, "application/json", "{\"ok\":false}");
            return;
        }
        Preferences p;
        p.begin("ih", false);
        p.putString("ssid", ssid);
        p.putString("pass", pass);
        p.end();
        apiServer->send(200, "application/json", "{\"ok\":true}");
        restartPending = true;
    });

    apiServer->on("/reset-wifi", HTTP_POST, []() {
        Preferences p;
        p.begin("ih", false);
        p.clear();
        p.end();
        apiServer->send(200, "application/json", "{\"ok\":true}");
        restartPending = true;
    });

    apiServer->onNotFound([]() {
        apiServer->send(404, "application/json", "{\"error\":\"not found\"}");
    });

    apiServer->begin();
    Serial.printf("[API] server on %s\n", wifi.getIP().toString().c_str());
}

void stopAPIServer() {
    if (!apiServer) return;
    apiServer->stop();
    delete apiServer;
    apiServer = nullptr;
    Serial.println(F("[API] server stopped"));
}
