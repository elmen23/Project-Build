#include <WiFi.h>
#include <WebServer.h>
#include "provisioning/WiFiProvisioning.h"
#include "hal/PWMManager.h"
#include "hal/ConfigStore.h"
#include "core/AppContext.h"
#include "core/ParamValidator.h"
#include "server/APIServer.h"

WiFiProvisioning wifi;
PWMManager pwm(PWM_PIN_A, PWM_PIN_B);
ConfigStore config;
AppContext ctx;
APIServer api(pwm, wifi, config, ctx);
WebServer server(80);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=== IH v5 ==="));

    ctx.params = ParamValidator::clamp(config.load());
    if (!pwm.begin(ctx.params)) {
        Serial.println(F("[WARN] PWM init failed — continuing without PWM"));
    }

    Serial.printf("[Main] ID: %s\n",
        String((uint32_t)(ESP.getEfuseMac() >> 24), HEX).c_str());

    wifi.begin(&server);
    api.begin(&server);
    server.begin();
    Serial.println(F("[Main] server started"));
}

void loop() {
    if (ctx.isRestartRequested()) {
        executeRestart();
    }

    wifi.handle();
    api.handle();
    server.handleClient();
    pwm.softStartTick();
}
