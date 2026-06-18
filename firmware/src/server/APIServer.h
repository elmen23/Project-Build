#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "hal/PWMManager.h"
#include "hal/ConfigStore.h"
#include "core/AppContext.h"
#include "provisioning/WiFiProvisioning.h"

class APIServer {
public:
    APIServer(PWMManager& pwm, WiFiProvisioning& wifi, ConfigStore& config, AppContext& ctx);
    void begin(WebServer* server);
    void handle();

private:
    PWMManager& _pwm;
    WiFiProvisioning& _wifi;
    ConfigStore& _config;
    AppContext& _ctx;
    WebServer* _server = nullptr;

    void _handleStatus();
    void _handleRoot();
    void _handleStart();
    void _handleStop();
    void _handleSet();
    void _handleWifiStatus();
    void _handleScan();
    void _handleConnect();
    void _handleResetWifi();
    void _handleNotFound();
};
