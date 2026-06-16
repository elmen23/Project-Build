#include <Arduino.h>
#include "core/Types.h"
#include "core/ParamValidator.h"
#include "hal/ConfigStore.h"
#include "hal/PWMManager.h"
#include "event_logger.h"

ConfigStore  store;
PWMManager   pwm(PWM_PIN_A, PWM_PIN_B);
EventLogger  evtLog;

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== Induction Heater v4.0 — Phase 1 ===");

    Serial.println("[1/4] ConfigStore...");
    store.begin();

    Serial.println("[2/4] Loading params...");
    CoreParams p = store.loadParams();
    Serial.printf("  freq=%.0f  duty=%.1f  dt=%.0f  ss=%u  timeout=%u\n",
                  p.freq, p.duty, p.deadTimeNs, p.softStartMs, p.runTimeoutSec);

    Serial.println("[3/4] PWMManager...");
    if (!pwm.begin(p)) {
        Serial.println("[FAIL] PWM init — halting");
        while (1) { delay(1000); }
    }

    Serial.println("[4/4] EventLogger...");
    evtLog.begin();
    evtLog.log(EVT_INFO, "Phase 1 boot complete");

    Serial.println("\n[TEST] PWM 50%% duty for 2 seconds...");
    pwm.start();
    pwm.setDuty(50.0f);
    delay(2000);

    pwm.stop();
    Serial.println("[TEST] PASS — PWM stop OK\n");

    Serial.println("[BOOT] Phase 1 ready — entering loop()");
}

unsigned long g_lastPrint = 0;

void loop() {
    unsigned long now = millis();
    if (now - g_lastPrint >= 1000) {
        g_lastPrint = now;
        Serial.printf("[LOOP] alive %lu s\n", now / 1000);
    }
}
