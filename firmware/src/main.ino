#include <WiFi.h>
#include <esp_task_wdt.h>
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

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println(F("=== IH v5 ==="));

    esp_task_wdt_init(10, true);
    esp_task_wdt_add(nullptr);

    ctx.params = ParamValidator::clamp(config.load());
    if (!pwm.begin(ctx.params)) {
        Serial.println(F("[CRIT] PWM init failed — halting"));
        while (true) { delay(1000); }
    }

    Serial.printf("[Main] ID: %s\n",
        String((uint32_t)(ESP.getEfuseMac() >> 24), HEX).c_str());

    wifi.begin();
}

void loop() {
    if (ctx.isRestartRequested()) {
        executeRestart();
    }

    wifi.handle();

    if (wifi.isProvisioned() && !api.isRunning()) {
        api.start(wifi.getIP());
    } else if (!wifi.isProvisioned() && api.isRunning()) {
        api.stop();
    }

    api.handle();
    pwm.softStartTick();
    esp_task_wdt_reset();
}
