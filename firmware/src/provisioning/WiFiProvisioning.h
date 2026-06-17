#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>

#define PROV_AP_SSID_PREFIX "IH-"
#define PROV_AP_TIMEOUT 180
#define PROV_STA_RETRY 15

class WiFiProvisioning {
public:
    WiFiProvisioning()
        : _state(STATE_INIT), _retries(0), _connStart(0), _dispStart(0),
          _dns(nullptr), _server(nullptr), _scanState(0),
          _testDone(false), _testOk(false) {}

    void begin();
    void handle();
    bool isProvisioned() const { return _state == STATE_STA_OK; }
    IPAddress getIP() const { return WiFi.localIP(); }

private:
    enum State : uint8_t {
        STATE_INIT,
        STATE_STA_WAIT,
        STATE_STA_OK,
        STATE_AP,
        STATE_AP_FAIL,
        STATE_AP_TEST,
        STATE_RESTART
    };

    State _state;
    uint8_t _retries;
    uint32_t _connStart;
    uint32_t _dispStart;
    String _ssid, _pass;
    String _testSSID, _testPass;

    uint8_t _scanState;
    String _scanCache;

    bool _testDone;
    bool _testOk;

    DNSServer* _dns;
    WebServer* _server;

    String _chipId() {
        return String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    }
    String _apSSID() { return String(PROV_AP_SSID_PREFIX) + _chipId(); }

    void _connectSTA(const String& ssid, const String& pass);
    void _startAP();
    void _stopAP();

    void _onRoot();
    void _onSave();
    void _onClear();
    void _onNotFound();

    void _servePage(const String& msg, bool isErr, const String& scanHtml = "");
    void _serveScanningPage();
    void _serveTestPage();
    void _serveSuccessPage();

    String _formHTML();
    void _buildScanCache(int n);
    bool _loadCreds();
    void _saveCreds();
    String _signalBars(int quality);
    String _esc(const String& s);
};

static const char P_TOP[] PROGMEM =
    "<!DOCTYPE html><html><head>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,sans-serif;background:#f5f5f5;padding:20px}"
    ".c{background:#fff;border-radius:12px;padding:24px;max-width:480px;margin:40px auto;text-align:center}"
    "h1{font-size:20px;margin:0 0 16px;color:#222}"
    ".left{text-align:left}"
    ".networks{margin-bottom:16px;text-align:left}"
    ".n{padding:10px 0;border-bottom:1px solid #eee;cursor:pointer}"
    ".n:hover{background:#f5f5f5;margin:0 -4px;padding:10px 4px}"
    ".s{font-weight:600;color:#333}"
    ".r{float:right;color:#999;font-size:13px;display:flex;align-items:center;height:24px}"
    ".ns{color:#999;font-size:14px;margin-bottom:12px;display:block}"
    "label{display:block;font-size:14px;font-weight:600;color:#555;margin:12px 0 4px;text-align:left}"
    "input{width:100%;padding:12px;border:1px solid #ddd;border-radius:8px;font-size:16px}"
    "input:focus{border-color:#007AFF;outline:none}"
    "button{background:#007AFF;color:#fff;border:none;border-radius:8px;padding:14px;"
    "font-size:16px;font-weight:600;width:100%;margin-top:20px;cursor:pointer}"
    "button:hover{background:#0056CC}"
    "button.d{background:#d32f2f;margin-top:8px}"
    "button.d:hover{background:#b71c1c}"
    ".e{color:#d32f2f;text-align:center;margin-top:12px;font-size:14px}"
    ".i{color:#1976d2;text-align:center;margin-top:12px;font-size:14px}"
    ".ip{font-size:13px;color:#666;margin:8px 0;text-align:center}"
    ".bars{display:inline-flex;align-items:flex-end;height:20px;vertical-align:middle}"
    ".bar{width:3px;margin:1px;border-radius:2px;background:#ddd;flex-shrink:0}"
    ".b0{height:8px}.b1{height:12px}.b2{height:16px}.b3{height:20px}"
    ".on.b0{background:#e53935}.on.b1{background:#fdd835}.on.b2{background:#8bc34a}.on.b3{background:#43a047}"
    ".sp{width:24px;height:24px;border:3px solid #eee;border-top-color:#007AFF;"
    "border-radius:50%;animation:s .8s linear infinite;margin:12px auto}"
    "@keyframes s{to{transform:rotate(360deg)}}"
    ".st{font-size:14px;color:#666;margin:8px 0}"
    ".mt{font-size:13px;color:#999}"
    "</style>"
    "<script>function p(n){document.getElementById('s').value=n}</script>"
    "</head><body><div class=c>";

static const char P_BOTTOM[] PROGMEM = "</div></body></html>";

static const char P_FORM[] PROGMEM =
    "<div class=left><form method=POST action='/'><label>SSID</label>"
    "<input id=s name=ssid placeholder='Network name' value='%s'>"
    "<label>Password</label>"
    "<input id=p name=pass type=password placeholder='Password'>"
    "<button>Connect</button></form>"
    "<form method=POST action='/clear'>"
    "<button class=d>Clear Credentials</button></form></div>";

static const char P_NET_ROW[] PROGMEM =
    "<div class=n onclick='p(\"%s\")'>"
    "<span class=s>%s</span>"
    "<span class=r>%s</span></div>";

void WiFiProvisioning::begin() {
    if (_loadCreds()) {
        Serial.print(F("Saved: "));
        Serial.println(_ssid);
        _state = STATE_STA_WAIT;
        WiFi.mode(WIFI_STA);
        _connectSTA(_ssid, _pass);
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

        if (_testDone && _testOk && (millis() - _dispStart) / 1000 >= 5) {
            _state = STATE_RESTART;
        }
        if (!_testDone && (millis() - _connStart) / 1000 >= PROV_AP_TIMEOUT) {
            Serial.println(F("AP timeout"));
            _state = STATE_RESTART;
        }
        break;

    case STATE_AP_TEST:
        _dns->processNextRequest();
        _server->handleClient();

        {
            wl_status_t s = WiFi.status();
            if (s == WL_CONNECTED && WiFi.localIP()) {
                Serial.print(F("Test OK: "));
                Serial.println(WiFi.localIP());
                _ssid = _testSSID;
                _pass = _testPass;
                _saveCreds();
                _testOk = true;
                _testDone = true;
                _state = STATE_AP;
                _dispStart = millis();
                return;
            }
            if ((millis() - _connStart) / 1000 >= PROV_STA_RETRY) {
                Serial.println(F("Test fail"));
                WiFi.disconnect(true, false);
                _testDone = true;
                _testOk = false;
                _state = STATE_AP;
                _dispStart = millis();
            }
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

void WiFiProvisioning::_connectSTA(const String& ssid, const String& pass) {
    WiFi.setHostname(_apSSID().c_str());
    WiFi.setAutoReconnect(false);
    WiFi.persistent(false);
    WiFi.begin(ssid.c_str(), pass.c_str());
    _connStart = millis();
}

void WiFiProvisioning::_startAP() {
    WiFi.persistent(false);
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
    _server->on("/clear", HTTP_POST, [this]() { _onClear(); });
    _server->onNotFound([this]() { _onNotFound(); });
    _server->begin();

    _connStart = millis();
    _scanState = 0;
}

void WiFiProvisioning::_stopAP() {
    if (_dns) { _dns->stop(); delete _dns; _dns = nullptr; }
    if (_server) { _server->stop(); delete _server; _server = nullptr; }
}

void WiFiProvisioning::_onRoot() {
    if (_state == STATE_AP_TEST) {
        _serveTestPage();
        return;
    }

    if (_testDone) {
        if (_testOk) {
            _serveSuccessPage();
            return;
        }
        _servePage(F("Could not connect. Check password."), true, _scanCache);
        _testDone = false;
        _ssid = _testSSID;
        _scanState = 0;
        _scanCache = "";
        return;
    }

    if (_state == STATE_AP_FAIL) {
        _servePage(F("Wrong credentials or network unreachable"), true, "");
        _state = STATE_AP;
        return;
    }

    if (_scanState == 0) {
        WiFi.scanNetworks(true);
        _scanState = 1;
        _serveScanningPage();
        return;
    }

    if (_scanState == 1) {
        int n = WiFi.scanComplete();
        if (n < 0) {
            _serveScanningPage();
            return;
        }
        _buildScanCache(n);
        WiFi.scanDelete();
        _scanState = 2;
    }

    _servePage("", false, _scanCache);
    _scanState = 0;
}

void WiFiProvisioning::_onSave() {
    _testSSID = _server->arg("ssid").substring(0, 32);
    _testPass = _server->arg("pass").substring(0, 64);

    if (_testSSID.length() == 0) {
        _servePage(F("Enter a network name"), true);
        return;
    }

    _testDone = false;
    _testOk = false;
    _state = STATE_AP_TEST;
    WiFi.mode(WIFI_AP_STA);
    _connectSTA(_testSSID, _testPass);
    Serial.printf("Test: %s\n", _testSSID.c_str());
    _serveTestPage();
}

void WiFiProvisioning::_onClear() {
    Preferences prefs;
    prefs.begin("ih", false);
    prefs.clear();
    prefs.end();
    _ssid = "";
    _pass = "";
    Serial.println(F("Cleared"));
    String html = FPSTR(P_TOP);
    html += F("<h1>Credentials Cleared</h1>"
        "<p class=st>Restarting...</p>"
        "<meta http-equiv='refresh' content='3'>");
    html += FPSTR(P_BOTTOM);
    _server->send(200, "text/html", html);
    _state = STATE_RESTART;
}

void WiFiProvisioning::_onNotFound() {
    _server->sendHeader("Location",
        String(F("http://")) + WiFi.softAPIP().toString(), true);
    _server->send(302, "text/plain", "");
}

void WiFiProvisioning::_servePage(const String& msg, bool isErr, const String& scanHtml) {
    String html;
    html.reserve(2048);
    html += FPSTR(P_TOP);
    if (scanHtml.length()) {
        html += F("<div class=networks>");
        html += scanHtml;
        html += F("</div>");
    }
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

void WiFiProvisioning::_serveScanningPage() {
    String html;
    html.reserve(512);
    html += FPSTR(P_TOP);
    html += F("<meta http-equiv='refresh' content='2'>"
        "<h1>Scanning Networks</h1>"
        "<div class=sp></div>"
        "<p class=st>Looking for WiFi networks...</p>");
    html += FPSTR(P_BOTTOM);
    _server->send(200, "text/html", html);
}

void WiFiProvisioning::_serveTestPage() {
    int elapsed = (millis() - _connStart) / 1000;
    String html;
    html.reserve(512);
    html += FPSTR(P_TOP);
    html += F("<meta http-equiv='refresh' content='2'>"
        "<h1>Connecting</h1>"
        "<div class=sp></div>"
        "<p class=st>Connecting to ");
    html += _esc(_testSSID);
    html += F("</p><p class=mt>");
    html += String(elapsed);
    html += F(" / ");
    html += String(PROV_STA_RETRY);
    html += F("s</p>");
    html += FPSTR(P_BOTTOM);
    _server->send(200, "text/html", html);
}

void WiFiProvisioning::_serveSuccessPage() {
    String html;
    html.reserve(512);
    html += FPSTR(P_TOP);
    html += F("<h1>Connected!</h1>"
        "<p class=ip>IP: ");
    html += WiFi.localIP().toString();
    html += F("</p>"
        "<p><a href='http://");
    html += WiFi.localIP().toString();
    html += F("/' target=_blank>Dashboard</a> (after restart)</p>"
        "<p class=st>Device will restart in 5 seconds...</p>");
    html += FPSTR(P_BOTTOM);
    _server->send(200, "text/html", html);
}

String WiFiProvisioning::_formHTML() {
    char buf[512];
    snprintf_P(buf, sizeof(buf), P_FORM, _esc(_ssid).c_str());
    return String(buf);
}

void WiFiProvisioning::_buildScanCache(int n) {
    if (n <= 0) {
        _scanCache = F("<span class=ns>No networks found</span>");
        return;
    }

    std::vector<int> idx(n);
    for (int i = 0; i < n; i++) idx[i] = i;
    std::sort(idx.begin(), idx.end(), [](int a, int b) {
        return WiFi.RSSI(a) > WiFi.RSSI(b);
    });

    std::vector<bool> skip(n, false);
    for (int i = 0; i < n; i++) {
        if (skip[i]) continue;
        for (int j = i + 1; j < n; j++) {
            if (WiFi.SSID(idx[i]) == WiFi.SSID(idx[j])) skip[j] = true;
        }
    }

    String html;
    int shown = 0;
    for (int i = 0; i < n && shown < 15; i++) {
        if (skip[i]) continue;
        String ssid = WiFi.SSID(idx[i]);
        if (ssid.length() == 0) continue;
        int rssi = WiFi.RSSI(idx[i]);
        int quality = constrain(2 * (rssi + 100), 0, 100);
        String bars = _signalBars(quality);
        char row[384];
        snprintf_P(row, sizeof(row), P_NET_ROW,
            _esc(ssid).c_str(), _esc(ssid).c_str(), bars.c_str());
        html += row;
        shown++;
    }

    _scanCache = html;
}

String WiFiProvisioning::_signalBars(int quality) {
    int bars = constrain((quality + 20) / 25, 0, 4);
    String h = "<span class=bars>";
    for (int i = 0; i < 4; i++) {
        h += "<span class='bar b";
        h += char('0' + i);
        h += "'";
        if (i < bars) h += " on></span>";
        else h += "></span>";
    }
    h += "</span>";
    return h;
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
    Preferences prefs;
    prefs.begin("ih", true);
    _ssid = prefs.getString("ssid", "");
    _pass = prefs.getString("pass", "");
    prefs.end();
    return _ssid.length() > 0;
}

void WiFiProvisioning::_saveCreds() {
    Preferences prefs;
    prefs.begin("ih", false);
    prefs.putString("ssid", _ssid);
    prefs.putString("pass", _pass);
    prefs.end();
    Serial.println(F("Saved"));
}
