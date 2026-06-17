#include "AppContext.h"

void executeRestart() {
    Serial.println(F("[App] restarting..."));
    delay(500);
    ESP.restart();
}
