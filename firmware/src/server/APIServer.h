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
    void start(const IPAddress& ip);
    void stop();
    void handle();
    bool isRunning() const { return _server != nullptr; }

private:
    PWMManager& _pwm;
    WiFiProvisioning& _wifi;
    ConfigStore& _config;
    AppContext& _ctx;
    WebServer* _server = nullptr;
};
