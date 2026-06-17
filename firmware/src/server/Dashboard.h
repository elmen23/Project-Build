#pragma once
#include <Arduino.h>
#include "core/AppContext.h"
#include "hal/PWMManager.h"
#include "provisioning/WiFiProvisioning.h"

String buildDashboard(PWMManager& pwm, AppContext& ctx, WiFiProvisioning& wifi);
