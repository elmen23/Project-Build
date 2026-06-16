#include "provisioning/WiFiProvisioning.h"

WiFiProvisioning provisioning;

void setup() {
    Serial.begin(115200);
    delay(200);
    provisioning.begin();
}

void loop() {
    provisioning.handle();
}
