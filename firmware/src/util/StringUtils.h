#pragma once
#include <Arduino.h>

String htmlEscape(const String& s);
String jsonEscape(const String& s);
String signalBars(int quality);
