#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n=== MINIMAL TEST ===");
    pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    Serial.printf("[LOOP] alive %lu ms\n", millis());
    delay(1000);
}
