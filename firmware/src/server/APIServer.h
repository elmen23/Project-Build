#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <memory>
#include "hal/PWMManager.h"
#include "hal/ConfigStore.h"
#include "hal/CTFeedback.h"
#include "hal/PLLController.h"
#include "core/AppContext.h"
#include "provisioning/WiFiProvisioning.h"

class APIServer {
public:
    APIServer(PWMManager& pwm, WiFiProvisioning& wifi, ConfigStore& config, AppContext& ctx, CTFeedback& ct, PLLController& pll);
    void start(const IPAddress& ip);
    void stop();
    void handle();
    bool isRunning() const { return _server != nullptr; }

private:
    PWMManager& _pwm;
    WiFiProvisioning& _wifi;
    ConfigStore& _config;
    AppContext& _ctx;
    CTFeedback& _ct;
    PLLController& _pll;
    std::unique_ptr<WebServer> _server;
};
