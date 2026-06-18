#include "StringUtils.h"

String htmlEscape(const String& s) {
    String out;
    out.reserve(s.length());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}


String jsonEscape(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((uint8_t)c < 0x20) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

String signalBars(int quality) {
    int bars = constrain((quality + 20) / 25, 0, 4);
    String h = "<span class=bars>";
    for (int i = 0; i < 4; i++) {
        h += "<span class='bar b";
        h += char('0' + i);
        h += "'";
        if (i < bars) h += " on></span>";
        else h += "></span>";
    }
    h += "</span>";
    return h;
}
