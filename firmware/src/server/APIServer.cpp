#include "APIServer.h"
#include "Dashboard.h"
#include "core/ParamValidator.h"
#include <ArduinoJson.h>
#include <Preferences.h>

APIServer::APIServer(PWMManager& pwm, WiFiProvisioning& wifi, ConfigStore& config, AppContext& ctx)
    : _pwm(pwm), _wifi(wifi), _config(config), _ctx(ctx) {}

void APIServer::begin(WebServer* server) {
    _server = server;
    if (!_server) return;

    _server->on("/", HTTP_GET, [this]() { _handleRoot(); });
    _server->on("/status", HTTP_GET, [this]() { _handleStatus(); });
    _server->on("/start", HTTP_GET, [this]() { _handleStart(); });
    _server->on("/stop", HTTP_GET, [this]() { _handleStop(); });
    _server->on("/set", HTTP_GET, [this]() { _handleSet(); });
    _server->on("/save", HTTP_POST, [this]() { _wifi.handleSave(); });
    _server->on("/clear", HTTP_POST, [this]() { _wifi.handleClear(); });
    _server->on("/wifi-status", HTTP_GET, [this]() { _handleWifiStatus(); });
    _server->on("/scan", HTTP_GET, [this]() { _handleScan(); });
    _server->on("/connect", HTTP_POST, [this]() { _handleConnect(); });
    _server->on("/reset-wifi", HTTP_POST, [this]() { _handleResetWifi(); });
    _server->onNotFound([this]() { _handleNotFound(); });
}

void APIServer::handle() {}

void APIServer::_handleRoot() {
    if (_wifi.isProvisioned()) {
        _server->send(200, "text/html", buildDashboard(_pwm, _ctx, _wifi));
    } else {
        _wifi.handleRoot();
    }
}

void APIServer::_handleStatus() {
    StaticJsonDocument<320> doc;

    doc["running"] = _pwm.isRunning();
    doc["softStart"] = _pwm.isSoftStarting();

    switch (_pwm.getState()) {
        case RunState::IDLE:       doc["state"] = "IDLE"; break;
        case RunState::SOFT_START: doc["state"] = "SOFT_START"; break;
        case RunState::RUNNING:    doc["state"] = "RUNNING"; break;
    }

    doc["freq"] = _ctx.params.freq;
    doc["duty"] = _pwm.getDuty();
    doc["dt"] = _ctx.params.deadTimeNs;
    doc["ss"] = _ctx.params.softStartMs;
    doc["ip"] = _wifi.getIP().toString();
    doc["rssi"] = WiFi.RSSI();

    String json;
    serializeJson(doc, json);
    _server->send(200, "application/json", json);
}

void APIServer::_handleStart() {
    if (!_pwm.isRunning())
        _pwm.start(_ctx.params.softStartMs);
    _server->sendHeader("Location", "/", true);
    _server->send(302, "text/plain", "");
}

void APIServer::_handleStop() {
    if (_pwm.isRunning())
        _pwm.stop();
    _server->sendHeader("Location", "/", true);
    _server->send(302, "text/plain", "");
}

void APIServer::_handleSet() {
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
}

void APIServer::_handleWifiStatus() {
    StaticJsonDocument<128> doc;
    doc["state"] = _wifi.isProvisioned() ? "connected" : "disconnected";
    doc["ip"] = _wifi.getIP().toString();
    doc["rssi"] = WiFi.RSSI();

    String json;
    serializeJson(doc, json);
    _server->send(200, "application/json", json);
}

void APIServer::_handleScan() {
    int n = WiFi.scanComplete();
    if (n == -2) {
        WiFi.scanNetworks(true);
        _server->send(200, "application/json", "{\"scanning\":true}");
    } else if (n == -1) {
        _server->send(200, "application/json", "{\"scanning\":true}");
    } else {
        StaticJsonDocument<1024> doc;
        JsonArray nets = doc.to<JsonArray>();
        for (int i = 0; i < n; i++) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;
            JsonObject net = nets.createNestedObject();
            net["ssid"] = ssid;
            net["rssi"] = WiFi.RSSI(i);
            net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        }
        WiFi.scanDelete();
        String json;
        serializeJson(doc, json);
        _server->send(200, "application/json", json);
    }
}

void APIServer::_handleConnect() {
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
}

void APIServer::_handleResetWifi() {
    {
        Preferences p;
        p.begin("ih", false);
        p.clear();
        p.end();
    }
    _server->send(200, "application/json", "{\"ok\":true}");
    _ctx.requestRestart();
}

void APIServer::_handleNotFound() {
    if (!_wifi.isProvisioned()) {
        _wifi.handleNotFound();
    } else {
        _server->send(404, "application/json", "{\"error\":\"not found\"}");
    }
}
