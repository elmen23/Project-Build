/*
 * IH-2000 — MCPWM BARE TEST
 * Just half-bridge PWM on GPIO18/19 immediately on boot.
 * No WiFi, no web — pure MCPWM test.
 */

#include <Arduino.h>
#include "driver/mcpwm.h"

#define GPIO_HIGH  GPIO_NUM_18
#define GPIO_LOW   GPIO_NUM_19

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n=== MCPWM BARE TEST ==="));

    /* --- Init MCPWM manually (no mcpwm_init) --- */
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, GPIO_HIGH);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, GPIO_LOW);

    mcpwm_stop(MCPWM_UNIT_0, MCPWM_TIMER_0);                  // ensure stopped

    mcpwm_set_frequency(MCPWM_UNIT_0, MCPWM_TIMER_0, 50000); // 50 kHz
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 50.0f);  // 50%
    mcpwm_set_duty_type(MCPWM_UNIT_0, MCPWM_TIMER_0,
                         MCPWM_OPR_A, MCPWM_DUTY_MODE_0);

    /* Enable dead time BEFORE starting */
    mcpwm_deadtime_enable(MCPWM_UNIT_0, MCPWM_TIMER_0,
                          MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE,
                          40, 40);   // 500 ns  (40 × 12.5 ns)

    mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);

    Serial.println(F("[OK] 50 kHz  DT=500 ns  GPIO18/19"));
    Serial.println(F("Measure GPIO18 ↔ GND with freq meter"));
}

void loop() {
    delay(10000);   // nothing — just sit here
}
