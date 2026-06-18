#include "APIServer.h"
#include "Dashboard.h"
#include "core/ParamValidator.h"
#include <Preferences.h>

static const char* _pllStateName(PLLController::State s) {
    switch (s) {
        case PLLController::IDLE:     return "IDLE";
        case PLLController::SWEEPING: return "SWEEP";
        case PLLController::PLL_LOCK: return "LOCK";
        case PLLController::PLL_UNLOCK: return "UNLOCK";
        default: return "?";
    }
}

APIServer::APIServer(PWMManager& pwm, WiFiProvisioning& wifi, ConfigStore& config, AppContext& ctx, CTFeedback& ct, PLLController& pll)
    : _pwm(pwm), _wifi(wifi), _config(config), _ctx(ctx), _ct(ct), _pll(pll) {}

void APIServer::start(const IPAddress& ip) {
    _server.reset(new WebServer(80));

    _server->on("/status", HTTP_GET, [this]() {
        String json;
        json.reserve(256);
        json += "{\"running\":";   json += _pwm.isRunning() ? "true" : "false";
        json += ",\"softStart\":"; json += _pwm.isSoftStarting() ? "true" : "false";
        json += ",\"state\":\"";   json += runStateName(_pwm.getState());
        json += "\",\"freq\":";    json += String(_ctx.params.freq, 0);
        json += ",\"duty\":";      json += String(_pwm.getDuty(), 1);
        json += ",\"dt\":";        json += String(_ctx.params.deadTimeNs, 0);
        json += ",\"ss\":";        json += String(_ctx.params.softStartMs);
        json += ",\"tankFreq\":";  json += String(_ct.getTankFrequency(), 0);
        json += ",\"tankAmp\":";   json += String(_ct.getTankAmplitude(), 3);
        json += ",\"pllState\":\""; json += _pllStateName(_pll.getState());
        json += "\",\"pllErr\":";  json += String(_pll.getPhaseError(), 3);
        json += ",\"ip\":\"";      json += _wifi.getIP().toString();
        json += "\",\"rssi\":";    json += String(WiFi.RSSI());
        json += "}";
        _server->send(200, "application/json", json);
    });

    _server->on("/", HTTP_GET, [this]() {
        _server->send(200, "text/html", buildDashboard(_pwm, _ctx, _wifi, _ct, _pll));
    });

    _server->on("/start", HTTP_GET, [this]() {
        if (!_pwm.isRunning())
            _pwm.start(_ctx.params.softStartMs);
        _server->sendHeader("Location", "/", true);
        _server->send(302, "text/plain", "");
    });

    _server->on("/stop", HTTP_GET, [this]() {
        if (_pwm.isRunning())
            _pwm.stop();
        _server->sendHeader("Location", "/", true);
        _server->send(302, "text/plain", "");
    });

    _server->on("/set", HTTP_GET, [this]() {
        CoreParams p = _ctx.params;
        if (_server->hasArg("f"))
            p.freq = _server->arg("f").toFloat();
        if (_server->hasArg("dt"))
            p.deadTimeNs = _server->arg("dt").toFloat();
        if (_server->hasArg("ss"))
            p.softStartMs = _server->arg("ss").toInt();
        p = ParamValidator::clamp(p);
        _ctx.params = p;
        _config.save(_ctx.params);
        _pwm.setFrequency(_ctx.params.freq);
        _pwm.setDeadTime(_ctx.params.deadTimeNs);
        _server->sendHeader("Location", "/", true);
        _server->send(302, "text/plain", "");
    });

    _server->on("/wifi-status", HTTP_GET, [this]() {
        String json;
        json.reserve(128);
        json += "{\"state\":\"";
        json += _wifi.isProvisioned() ? "connected" : "disconnected";
        json += "\",\"ip\":\"";   json += _wifi.getIP().toString();
        json += "\",\"rssi\":";   json += String(WiFi.RSSI());
        json += "}";
        _server->send(200, "application/json", json);
    });

    _server->on("/scan", HTTP_GET, [this]() {
        int n = WiFi.scanComplete();
        if (n == -2) {
            WiFi.scanNetworks(true);
            _server->send(200, "application/json", "{\"scanning\":true}");
        } else if (n == -1) {
            _server->send(200, "application/json", "{\"scanning\":true}");
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
            _server->send(200, "application/json", json);
        }
    });

    _server->on("/connect", HTTP_POST, [this]() {
        String ssid = _server->arg("ssid").substring(0, 32);
        String pass = _server->arg("pass").substring(0, 64);
        if (ssid.length() == 0) {
            _server->send(400, "application/json", "{\"ok\":false}");
            return;
        }
        Preferences p;
        p.begin("ih", false);
        p.putString("ssid", ssid);
        p.putString("pass", pass);
        p.end();
        _server->send(200, "application/json", "{\"ok\":true}");
        _ctx.requestRestart();
    });

    _server->on("/reset-wifi", HTTP_POST, [this]() {
        {
            Preferences p;
            p.begin("ih", false);
            p.clear();
            p.end();
        }
        _server->send(200, "application/json", "{\"ok\":true}");
        _ctx.requestRestart();
    });

    _server->onNotFound([this]() {
        _server->send(404, "application/json", "{\"error\":\"not found\"}");
    });

    _server->begin();
    Serial.printf("[API] server on %s\n", ip.toString().c_str());
}

void APIServer::stop() {
    if (!_server) return;
    _server->stop();
    _server.reset();
    Serial.println(F("[API] server stopped"));
}

void APIServer::handle() {
    if (_server) _server->handleClient();
}
