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

String buildDashboard();
void startAPIServer();
void stopAPIServer();

String buildDashboard() {
    String s, stateStr;
    switch (pwm.getState()) {
        case RunState::IDLE:       stateStr = "IDLE"; break;
        case RunState::SOFT_START: stateStr = "SOFT START"; break;
        case RunState::RUNNING:    stateStr = "RUNNING"; break;
        case RunState::STOPPING:   stateStr = "STOPPING"; break;
    }
    s.reserve(1024);
    s += "<!DOCTYPE html><html><head><meta charset=UTF-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
         "<meta http-equiv=refresh content='3'>"
         "<title>IH</title><style>"
         "*{box-sizing:border-box;margin:0;padding:0}"
         "body{font-family:monospace;padding:20px;background:#111;color:#0f0}"
         "a{text-decoration:none}"
         ".s{margin-bottom:16px;padding:12px;border:1px solid #333}"
         ".r{display:flex;justify-content:space-between;padding:4px 0}"
         ".l{color:#888}.v{color:#0f0}"
         "button{padding:8px 24px;font-size:16px;cursor:pointer;border:0;border-radius:4px}"
         ".bs{background:#0a0;color:#fff}"
         ".bp{background:#a00;color:#fff}"
         ".ba{background:#00a;color:#fff;padding:6px 16px;border:0;border-radius:4px}"
         "input{background:#000;border:1px solid #333;color:#0f0;padding:6px;width:80px}"
         ".fr{display:flex;gap:8px;align-items:center;margin:4px 0}"
         ".fr lb{width:100px;color:#888}"
         ".sm{text-align:center;margin-top:8px}"
         "</style></head><body><h1>IH</h1><div class=s>"
         "<div class=r><span class=l>Status</span><span class=v>" + stateStr + "</span></div>"
         "<div class=r><span class=l>Freq</span><span class=v>" + String(params.freq, 0) + " Hz</span></div>"
         "<div class=r><span class=l>Duty</span><span class=v>" + String(pwm.getDuty(), 1) + "% (" + String(params.duty, 1) + "%)</span></div>"
         "<div class=r><span class=l>DT</span><span class=v>" + String(params.deadTimeNs, 0) + " ns</span></div>"
         "<div class=r><span class=l>SS</span><span class=v>" + String(params.softStartMs) + " ms</span></div>"
         "<div class=r><span class=l>IP</span><span class=v>" + wifi.getIP().toString() + "</span></div>"
         "<div class=r><span class=l>RSSI</span><span class=v>" + String(WiFi.RSSI()) + " dBm</span></div>"
         "</div>"
         "<div class=s style=text-align:center>"
         "<a href=/start><button class=bs>Start</button></a> "
         "<a href=/stop><button class=bp>Stop</button></a>"
         "</div>"
         "<div class=s>"
         "<form action=/set method=GET style=display:flex;flex-direction:column;gap:6px>"
         "<div class=fr><lb>Freq (Hz)</lb><input name=f type=number value=" + String(params.freq, 0) + "></div>"
         "<div class=fr><lb>Duty (%)</lb><input name=d type=number step=0.1 value=" + String(params.duty, 1) + "></div>"
         "<div class=fr><lb>DT (ns)</lb><input name=dt type=number value=" + String(params.deadTimeNs, 0) + "></div>"
         "<div class=fr><lb>SS (ms)</lb><input name=ss type=number value=" + String(params.softStartMs) + "></div>"
         "<div class=sm><button class=ba type=submit>Apply</button></div>"
         "</form></div></body></html>";
    return s;
}

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
        apiServer->send(200, "text/html", buildDashboard());
    });

    apiServer->on("/start", HTTP_GET, []() {
        if (!pwm.isRunning())
            pwm.start(params.duty, params.softStartMs);
        apiServer->sendHeader("Location", "/", true);
        apiServer->send(302, "text/plain", "");
    });

    apiServer->on("/stop", HTTP_GET, []() {
        if (pwm.isRunning())
            pwm.stop();
        apiServer->sendHeader("Location", "/", true);
        apiServer->send(302, "text/plain", "");
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
        pwm.setDuty(params.duty);
        apiServer->sendHeader("Location", "/", true);
        apiServer->send(302, "text/plain", "");
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
