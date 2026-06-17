#include <WebServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "provisioning/WiFiProvisioning.h"
#include "hal/PWMManager.h"
#include "hal/ConfigStore.h"
#include "core/AppContext.h"
#include "core/ParamValidator.h"

WiFiProvisioning wifi;
PWMManager pwm(PWM_PIN_A, PWM_PIN_B);
ConfigStore config;
AppContext ctx;

String buildDashboard();
void startAPIServer();
void stopAPIServer();

String buildDashboard() {
    String s;
    s.reserve(1024);
    s += "<!DOCTYPE html><html><head><meta charset=UTF-8>"
         "<meta name=viewport content='width=device-width,initial-scale=1'>"
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
         "<div class=r><span class=l>Status</span><span class=v id=s>" + String(runStateName(pwm.getState())) + "</span></div>"
         "<div class=r><span class=l>Freq</span><span class=v id=f>" + String(ctx.params.freq, 0) + " Hz</span></div>"
         "<div class=r><span class=l>Duty</span><span class=v id=d>" + String(pwm.getDuty(), 1) + "% (" + String(ctx.params.duty, 1) + "%)</span></div>"
         "<div class=r><span class=l>DT</span><span class=v id=dt>" + String(ctx.params.deadTimeNs, 0) + " ns</span></div>"
         "<div class=r><span class=l>SS</span><span class=v id=ss>" + String(ctx.params.softStartMs) + " ms</span></div>"
         "<div class=r><span class=l>IP</span><span class=v id=ip>" + wifi.getIP().toString() + "</span></div>"
         "<div class=r><span class=l>RSSI</span><span class=v id=r>" + String(WiFi.RSSI()) + " dBm</span></div>"
         "</div>"
         "<div class=s style=text-align:center>"
         "<a href=/start><button class=bs>Start</button></a> "
         "<a href=/stop><button class=bp>Stop</button></a>"
         "</div>"
         "<div class=s>"
         "<form action=/set method=GET style=display:flex;flex-direction:column;gap:6px>"
         "<div class=fr><lb>Freq (Hz)</lb><input name=f type=number value=" + String(ctx.params.freq, 0) + "></div>"
         "<div class=fr><lb>Duty (%)</lb><input name=d type=number step=0.1 value=" + String(ctx.params.duty, 1) + "></div>"
         "<div class=fr><lb>DT (ns)</lb><input name=dt type=number value=" + String(ctx.params.deadTimeNs, 0) + "></div>"
         "<div class=fr><lb>SS (ms)</lb><input name=ss type=number value=" + String(ctx.params.softStartMs) + "></div>"
         "<div class=sm><button class=ba type=submit>Apply</button></div>"
         "</form></div>"
         "<script>"
         "function p(){fetch('/status').then(r=>r.json()).then(d=>{"
         "document.getElementById('s').innerText=d.state;"
         "document.getElementById('f').innerText=d.freq+' Hz';"
         "document.getElementById('d').innerText=d.duty+'% ('+d.dutyTarget+'%)';"
         "document.getElementById('dt').innerText=d.dt+' ns';"
         "document.getElementById('ss').innerText=d.ss+' ms';"
         "document.getElementById('ip').innerText=d.ip;"
         "document.getElementById('r').innerText=d.rssi+' dBm'})"
         ".catch(function(){})}"
         "setInterval(p,2000);p()"
         "</script>"
         "</body></html>";
    return s;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=== IH v5 ==="));

    esp_task_wdt_init(10, true);
    esp_task_wdt_add(nullptr);

    ctx.params = ParamValidator::clamp(config.load());
    if (!pwm.begin(ctx.params)) {
        Serial.println(F("[CRIT] PWM init failed — halting"));
        while (true) { delay(1000); }
    }

    Serial.printf("[Main] ID: %s\n",
        String((uint32_t)(ESP.getEfuseMac() >> 24), HEX).c_str());

    wifi.begin();
}

void loop() {
    if (ctx.isRestartRequested()) {
        executeRestart();
    }

    wifi.handle();

    if (wifi.isProvisioned() && ctx.apiServer == nullptr) {
        startAPIServer();
    } else if (!wifi.isProvisioned() && ctx.apiServer != nullptr) {
        stopAPIServer();
    }

    if (ctx.apiServer) {
        ctx.apiServer->handleClient();
    }

    pwm.softStartTick();
    esp_task_wdt_reset();
}

void startAPIServer() {
    ctx.apiServer = new WebServer(80);

    ctx.apiServer->on("/status", HTTP_GET, []() {
        String json;
        json.reserve(256);
        json += "{\"running\":";   json += pwm.isRunning() ? "true" : "false";
        json += ",\"softStart\":"; json += pwm.isSoftStarting() ? "true" : "false";
        json += ",\"state\":\"";   json += runStateName(pwm.getState());
        json += "\",\"freq\":";    json += String(ctx.params.freq, 0);
        json += ",\"duty\":";      json += String(pwm.getDuty(), 1);
        json += ",\"dt\":";        json += String(ctx.params.deadTimeNs, 0);
        json += ",\"ss\":";        json += String(ctx.params.softStartMs);
        json += ",\"dutyTarget\":"; json += String(ctx.params.duty, 1);
        json += ",\"ip\":\"";      json += wifi.getIP().toString();
        json += "\",\"rssi\":";    json += String(WiFi.RSSI());
        json += "}";
        ctx.apiServer->send(200, "application/json", json);
    });

    ctx.apiServer->on("/", HTTP_GET, []() {
        ctx.apiServer->send(200, "text/html", buildDashboard());
    });

    ctx.apiServer->on("/start", HTTP_GET, []() {
        if (!pwm.isRunning())
            pwm.start(ctx.params.duty, ctx.params.softStartMs);
        ctx.apiServer->sendHeader("Location", "/", true);
        ctx.apiServer->send(302, "text/plain", "");
    });

    ctx.apiServer->on("/stop", HTTP_GET, []() {
        if (pwm.isRunning())
            pwm.stop();
        ctx.apiServer->sendHeader("Location", "/", true);
        ctx.apiServer->send(302, "text/plain", "");
    });

    ctx.apiServer->on("/set", HTTP_GET, []() {
        CoreParams p = ctx.params;
        if (ctx.apiServer->hasArg("f"))
            p.freq = ctx.apiServer->arg("f").toFloat();
        if (ctx.apiServer->hasArg("d"))
            p.duty = ctx.apiServer->arg("d").toFloat();
        if (ctx.apiServer->hasArg("dt"))
            p.deadTimeNs = ctx.apiServer->arg("dt").toFloat();
        if (ctx.apiServer->hasArg("ss"))
            p.softStartMs = ctx.apiServer->arg("ss").toInt();
        p = ParamValidator::clamp(p);
        ctx.params = p;
        config.save(ctx.params);
        pwm.setFrequency(ctx.params.freq);
        pwm.setDeadTime(ctx.params.deadTimeNs);
        pwm.setDuty(ctx.params.duty);
        ctx.apiServer->sendHeader("Location", "/", true);
        ctx.apiServer->send(302, "text/plain", "");
    });

    ctx.apiServer->on("/wifi-status", HTTP_GET, []() {
        String json;
        json.reserve(128);
        json += "{\"state\":\"";
        json += wifi.isProvisioned() ? "connected" : "disconnected";
        json += "\",\"ip\":\"";   json += wifi.getIP().toString();
        json += "\",\"rssi\":";   json += String(WiFi.RSSI());
        json += "}";
        ctx.apiServer->send(200, "application/json", json);
    });

    ctx.apiServer->on("/scan", HTTP_GET, []() {
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true);
            ctx.apiServer->send(200, "application/json", "{\"scanning\":true}");
        } else if (n == -1) {
            ctx.apiServer->send(200, "application/json", "{\"scanning\":true}");
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
            ctx.apiServer->send(200, "application/json", json);
        }
    });

    ctx.apiServer->on("/connect", HTTP_POST, []() {
        String ssid = ctx.apiServer->arg("ssid").substring(0, 32);
        String pass = ctx.apiServer->arg("pass").substring(0, 64);
        if (ssid.length() == 0) {
            ctx.apiServer->send(400, "application/json", "{\"ok\":false}");
            return;
        }
        Preferences p;
        p.begin("ih", false);
        p.putString("ssid", ssid);
        p.putString("pass", pass);
        p.end();
        ctx.apiServer->send(200, "application/json", "{\"ok\":true}");
        ctx.requestRestart();
    });

    ctx.apiServer->on("/reset-wifi", HTTP_POST, []() {
        {
            Preferences p;
            p.begin("ih", false);
            p.clear();
            p.end();
        }
        ctx.apiServer->send(200, "application/json", "{\"ok\":true}");
        ctx.requestRestart();
    });

    ctx.apiServer->onNotFound([]() {
        ctx.apiServer->send(404, "application/json", "{\"error\":\"not found\"}");
    });

    ctx.apiServer->begin();
    Serial.printf("[API] server on %s\n", wifi.getIP().toString().c_str());
}

void stopAPIServer() {
    if (!ctx.apiServer) return;
    ctx.apiServer->stop();
    delete ctx.apiServer;
    ctx.apiServer = nullptr;
    Serial.println(F("[API] server stopped"));
}
