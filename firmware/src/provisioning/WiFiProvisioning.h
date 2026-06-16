#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>

#define PROV_AP_SSID_PREFIX "IH-"
#define PROV_AP_TIMEOUT 180
#define PROV_STA_RETRY 15

class WiFiProvisioning {
public:
    WiFiProvisioning()
        : _state(STATE_INIT), _retries(0), _connStart(0),
          _dns(nullptr), _server(nullptr) {}

    void begin();
    void handle();

private:
    enum State : uint8_t {
        STATE_INIT,
        STATE_STA_WAIT,
        STATE_STA_OK,
        STATE_AP,
        STATE_AP_FAIL,
        STATE_RESTART
    };

    State _state;
    uint8_t _retries;
    uint32_t _connStart;
    String _ssid, _pass;

    DNSServer* _dns;
    WebServer* _server;
    Preferences _prefs;

    String _chipId() {
        return String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    }
    String _apSSID() { return String(PROV_AP_SSID_PREFIX) + _chipId(); }

    void _connectSTA();
    void _startAP();
    void _stopAP();

    void _onRoot();
    void _onSave();
    void _onNotFound();

    void _servePage(const String& msg, bool isErr);
    String _formHTML();
    bool _loadCreds();
    void _saveCreds();
    String _esc(const String& s);
};

static const char P_TOP[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,sans-serif;background:#f5f5f5;padding:20px}"
    ".c{background:#fff;border-radius:12px;padding:24px;max-width:480px;margin:40px auto}"
    "h1{font-size:20px;margin:0 0 16px;color:#222}"
    "label{display:block;font-size:14px;font-weight:600;color:#555;margin:12px 0 4px}"
    "input{width:100%;padding:12px;border:1px solid #ddd;border-radius:8px;font-size:16px}"
    "input:focus{border-color:#007AFF;outline:none}"
    "button{background:#007AFF;color:#fff;border:none;border-radius:8px;padding:14px;"
    "font-size:16px;font-weight:600;width:100%;margin-top:20px;cursor:pointer}"
    "button:hover{background:#0056CC}"
    ".e{color:#d32f2f;text-align:center;margin-top:12px;font-size:14px}"
    ".i{color:#1976d2;text-align:center;margin-top:12px;font-size:14px}"
    "</style></head><body><div class=c><h1>IH Setup</h1>";

static const char P_BOTTOM[] PROGMEM = "</div></body></html>";

static const char P_FORM[] PROGMEM =
    "<form method=POST action='/'><label>SSID</label>"
    "<input id=s name=ssid placeholder='Network name' value='%s'>"
    "<label>Password</label>"
    "<input id=p name=pass type=password placeholder='Password'>"
    "<button>Connect</button></form>";

void WiFiProvisioning::begin() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=== IH v4 ==="));
    Serial.print(F("ID: "));
    Serial.println(_chipId());

    _prefs.begin("ih", false);

    if (_loadCreds()) {
        Serial.print(F("Saved: "));
        Serial.println(_ssid);
        _state = STATE_STA_WAIT;
        WiFi.mode(WIFI_STA);
        _connectSTA();
    } else {
        Serial.println(F("No creds -> AP"));
        _state = STATE_AP;
        _startAP();
    }
}

void WiFiProvisioning::handle() {
    switch (_state) {

    case STATE_STA_WAIT: {
        wl_status_t s = WiFi.status();
        if (s == WL_CONNECTED && WiFi.localIP()) {
            Serial.print(F("OK: "));
            Serial.println(WiFi.localIP());
            _state = STATE_STA_OK;
            WiFi.setAutoReconnect(true);
            return;
        }
        if ((millis() - _connStart) / 1000 >= PROV_STA_RETRY) {
            Serial.println(F("STA fail -> AP"));
            _state = STATE_AP;
            _startAP();
        }
        break;
    }

    case STATE_STA_OK:
        break;

    case STATE_AP:
    case STATE_AP_FAIL:
        _dns->processNextRequest();
        _server->handleClient();
        if ((millis() - _connStart) / 1000 >= PROV_AP_TIMEOUT) {
            Serial.println(F("AP timeout"));
            _state = STATE_RESTART;
        }
        break;

    case STATE_RESTART:
        Serial.println(F("Restart..."));
        delay(500);
        ESP.restart();
        break;

    default:
        break;
    }
}

void WiFiProvisioning::_connectSTA() {
    WiFi.setHostname(_apSSID().c_str());
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    WiFi.begin(_ssid.c_str(), _pass.c_str());
    _connStart = millis();
}

void WiFiProvisioning::_startAP() {
    _dns = new DNSServer();
    _server = new WebServer(80);

    String name = _apSSID();
    Serial.printf("AP: %s\n", name.c_str());

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(name.c_str());
    delay(500);

    IPAddress ip = WiFi.softAPIP();
    Serial.printf("IP: %s\n", ip.toString().c_str());

    _dns->setErrorReplyCode(DNSReplyCode::NoError);
    _dns->start(53, "*", ip);

    _server->on("/", HTTP_GET, [this]() { _onRoot(); });
    _server->on("/", HTTP_POST, [this]() { _onSave(); });
    _server->onNotFound([this]() { _onNotFound(); });
    _server->begin();

    _connStart = millis();
}

void WiFiProvisioning::_stopAP() {
    if (_dns) { _dns->stop(); delete _dns; _dns = nullptr; }
    if (_server) { _server->stop(); delete _server; _server = nullptr; }
}

void WiFiProvisioning::_onRoot() {
    if (_state == STATE_AP_FAIL) {
        _servePage(F("Wrong credentials or network unreachable"), true);
        _state = STATE_AP;
    } else {
        _servePage("", false);
    }
}

void WiFiProvisioning::_onSave() {
    _ssid = _server->arg("ssid");
    _pass = _server->arg("pass");

    if (_ssid.length() == 0) {
        _servePage(F("Enter a network name"), true);
        return;
    }

    _saveCreds();
    _servePage(F("Saved! Restarting..."), false);
    _state = STATE_RESTART;
}

void WiFiProvisioning::_onNotFound() {
    _server->sendHeader("Location",
        String(F("http://")) + WiFi.softAPIP().toString(), true);
    _server->send(302, "text/plain", "");
    _server->client().stop();
}

void WiFiProvisioning::_servePage(const String& msg, bool isErr) {
    String html;
    html.reserve(1024);
    html += FPSTR(P_TOP);
    html += _formHTML();
    if (msg.length()) {
        html += F("<div class='");
        html += isErr ? 'e' : 'i';
        html += "'>";
        html += msg;
        html += F("</div>");
    }
    html += FPSTR(P_BOTTOM);
    _server->send(200, "text/html", html);
}

String WiFiProvisioning::_formHTML() {
    char buf[512];
    _loadCreds();
    snprintf_P(buf, sizeof(buf), P_FORM, _esc(_ssid).c_str());
    return String(buf);
}

String WiFiProvisioning::_esc(const String& s) {
    String out;
    out.reserve(s.length());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

bool WiFiProvisioning::_loadCreds() {
    _ssid = _prefs.getString("ssid", "");
    _pass = _prefs.getString("pass", "");
    return _ssid.length() > 0;
}

void WiFiProvisioning::_saveCreds() {
    _prefs.putString("ssid", _ssid);
    _prefs.putString("pass", _pass);
    _prefs.end();
    Serial.println(F("Saved"));
}
