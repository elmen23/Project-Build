#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include <memory>

#define PROV_AP_SSID_PREFIX "IH-"
#define PROV_AP_TIMEOUT 180
#define PROV_STA_RETRY 15

class WiFiProvisioning {
public:
    WiFiProvisioning();
    void begin(WebServer* server);
    void handle();
    bool isProvisioned() const { return _state == STATE_STA_OK; }
    IPAddress getIP() const { return WiFi.localIP(); }

    void handleRoot();
    void handleSave();
    void handleClear();
    void handleNotFound();
    String apSSID() const { return _apSSID(); }

private:
    enum State : uint8_t {
        STATE_INIT,
        STATE_STA_WAIT,
        STATE_STA_OK,
        STATE_AP,
        STATE_AP_TEST,
        STATE_RESTART
    };

    enum class ScanState : uint8_t { IDLE, SCANNING, DONE };

    WebServer* _server = nullptr;
    State _state;
    uint8_t _retries;
    uint32_t _connStart;
    uint32_t _dispStart;
    String _ssid, _pass;
    String _testSSID, _testPass;
    ScanState _scanState;
    String _scanCache;
    bool _testDone;
    bool _testOk;
    std::unique_ptr<DNSServer> _dns;

    String _chipId();
    String _apSSID();
    void _connectSTA(const String& ssid, const String& pass);
    void _startAP();
    void _servePage(const String& msg, bool isErr, const String& scanHtml = "");
    void _serveScanningPage();
    void _serveTestPage();
    void _serveSuccessPage();
    String _formHTML();
    void _buildScanCache(int n);
    bool _loadCreds();
    void _saveCreds();
};
