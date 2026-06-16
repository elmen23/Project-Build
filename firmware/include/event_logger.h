#pragma once
/**
 * ═══════════════════════════════════════════════════════════════════
 *  event_logger.h  —  v3.0
 *  Circular event log stored in NVS (Preferences).
 *  Stores up to MAX_EVENTS entries with timestamp (millis) and message.
 * ═══════════════════════════════════════════════════════════════════
 */

#include <Preferences.h>
#include <Arduino.h>

#define EVT_NS          "event_log"
#define EVT_KEY_COUNT   "count"
#define EVT_KEY_HEAD    "head"
#define EVT_MAX_EVENTS  50
#define EVT_MSG_LEN     64

enum EventType : uint8_t {
  EVT_START = 0,
  EVT_STOP,
  EVT_EMERGENCY,
  EVT_TIMEOUT,
  EVT_PARAM_CHANGE,
  EVT_WIFI_CONNECT,
  EVT_WIFI_DISCONNECT,
  EVT_ERROR,
  EVT_INFO
};

static const char* eventTypeName(EventType t) {
  switch (t) {
    case EVT_START:           return "START";
    case EVT_STOP:            return "STOP";
    case EVT_EMERGENCY:       return "EMERGENCY";
    case EVT_TIMEOUT:         return "TIMEOUT";
    case EVT_PARAM_CHANGE:    return "PARAM";
    case EVT_WIFI_CONNECT:    return "WIFI_UP";
    case EVT_WIFI_DISCONNECT: return "WIFI_DOWN";
    case EVT_ERROR:           return "ERROR";
    case EVT_INFO:            return "INFO";
    default:                  return "UNKNOWN";
  }
}

struct LogEntry {
  uint32_t  timestamp;      // millis() at event time
  EventType type;
  char      message[EVT_MSG_LEN];
};

class EventLogger {
public:
  void begin() {
    prefs.begin(EVT_NS, false);
    count = prefs.getUInt(EVT_KEY_COUNT, 0);
    head  = prefs.getUInt(EVT_KEY_HEAD, 0);
    if (head >= EVT_MAX_EVENTS) head = 0;
    prefs.end();
  }

  // Log an event (thread-safe wrapper recommended in caller)
  void log(EventType type, const char* fmt, ...) {
    LogEntry e;
    e.timestamp = millis();
    e.type      = type;

    va_list args;
    va_start(args, fmt);
    vsnprintf(e.message, EVT_MSG_LEN, fmt, args);
    va_end(args);
    e.message[EVT_MSG_LEN - 1] = '\0';

    char key[8];
    snprintf(key, sizeof(key), "e%u", head);

    prefs.begin(EVT_NS, false);
    prefs.putBytes(key, &e, sizeof(e));

    head++;
    if (head >= EVT_MAX_EVENTS) head = 0;
    if (count < EVT_MAX_EVENTS) count++;

    prefs.putUInt(EVT_KEY_COUNT, count);
    prefs.putUInt(EVT_KEY_HEAD, head);
    prefs.end();

    Serial.printf("[Event] %s: %s\n", eventTypeName(type), e.message);
  }

  // Serialize all events as JSON array (oldest first)
  String toJSON() const {
    Preferences p;
    if (!p.begin(EVT_NS, true)) return "[]";

    uint32_t c = p.getUInt(EVT_KEY_COUNT, 0);
    uint32_t h = p.getUInt(EVT_KEY_HEAD, 0);
    if (c == 0) { p.end(); return "[]"; }

    String json = "[";
    uint32_t start = (c >= EVT_MAX_EVENTS) ? h : 0;
    for (uint32_t i = 0; i < c; i++) {
      uint32_t idx = (start + i) % EVT_MAX_EVENTS;
      char key[8];
      snprintf(key, sizeof(key), "e%u", idx);
      LogEntry e;
      size_t len = p.getBytes(key, &e, sizeof(e));
      if (len != sizeof(e)) continue;

      if (i > 0) json += ",";
      // Escape JSON string
      String msg = e.message;
      msg.replace("\\", "\\\\");
      msg.replace("\"", "\\\"");
      msg.replace("\n", "\\n");
      msg.replace("\r", "\\r");

      char buf[256];
      snprintf(buf, sizeof(buf),
               "{\"ts\":%u,\"type\":\"%s\",\"msg\":\"%s\"}",
               e.timestamp, eventTypeName(e.type), msg.c_str());
      json += buf;
    }
    p.end();
    json += "]";
    return json;
  }

  void clear() {
    prefs.begin(EVT_NS, false);
    prefs.clear();
    count = 0;
    head  = 0;
    prefs.end();
  }

private:
  mutable Preferences prefs;
  uint32_t count = 0;
  uint32_t head  = 0;   // next write position
};
