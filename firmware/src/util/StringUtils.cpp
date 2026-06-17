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
