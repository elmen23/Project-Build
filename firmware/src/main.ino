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

void startAPIServer();
void stopAPIServer();

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
        json += "{\"running\":";   json += pwm.isRunning() ? "true" : "false";
        json += ",\"softStart\":"; json += pwm.isSoftStarting() ? "true" : "false";
        json += ",\"freq\":";      json += String(params.freq, 0);
        json += ",\"duty\":";      json += String(pwm.getDuty(), 1);
        json += ",\"dt\":";        json += String(params.deadTimeNs, 0);
        json += ",\"ss\":";        json += String(params.softStartMs);
        json += ",\"wifi\":{\"state\":\"";
        json += wifi.isProvisioned() ? "connected" : "disconnected";
        json += "\",\"ip\":\"";    json += wifi.getIP().toString();
        json += "\",\"rssi\":";    json += String(WiFi.RSSI());
        json += "}}";
        apiServer->send(200, "application/json", json);
    });

    apiServer->on("/start", HTTP_GET, []() {
        if (pwm.isRunning()) {
            apiServer->send(200, "application/json", "{\"ok\":false,\"msg\":\"already running\"}");
            return;
        }
        pwm.start(params.duty, params.softStartMs);
        apiServer->send(200, "application/json", "{\"ok\":true}");
    });

    apiServer->on("/stop", HTTP_GET, []() {
        pwm.stop();
        apiServer->send(200, "application/json", "{\"ok\":true}");
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
        if (pwm.isRunning())
            pwm.setDuty(params.duty);

        String json = config.toJSON(params);
        apiServer->send(200, "application/json", json);
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
