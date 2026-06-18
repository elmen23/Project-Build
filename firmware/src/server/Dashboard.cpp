#include "Dashboard.h"

static const char* _pllStateName(PLLController::State s) {
    switch (s) {
        case PLLController::IDLE:     return "IDLE";
        case PLLController::SWEEPING: return "SWEEP";
        case PLLController::PLL_LOCK: return "LOCK";
        case PLLController::PLL_UNLOCK: return "UNLOCK";
        default: return "?";
    }
}

String buildDashboard(PWMManager& pwm, AppContext& ctx, WiFiProvisioning& wifi, CTFeedback& ct, PLLController& pll) {
    String s;
    s.reserve(1024);
    s += F("<!DOCTYPE html><html><head><meta charset=UTF-8>"
           "<meta name=viewport content='width=device-width,initial-scale=1'>"
           "<title>IH</title><style>"
           "*{box-sizing:border-box;margin:0;padding:0}"
           "body{font-family:monospace;padding:20px;background:#111;color:#0f0}"
           "a{text-decoration:none}"
           ".s{margin-bottom:16px;padding:12px;border:1px solid #333}"
           ".r{display:flex;justify-content:space-between;padding:4px 0}"
           ".l{color:#888}.v{color:#0f0}"
           "button{padding:8px 24px;font-size:16px;cursor:pointer;border:0;border-radius:4px}"
           ".bs{background:#0a0;color:#fff}"
           ".bp{background:#a00;color:#fff}"
           ".ba{background:#00a;color:#fff;padding:6px 16px;border:0;border-radius:4px}"
           "input{background:#000;border:1px solid #333;color:#0f0;padding:6px;width:80px}"
           ".fr{display:flex;gap:8px;align-items:center;margin:4px 0}"
           ".fr lb{width:100px;color:#888}"
           ".sm{text-align:center;margin-top:8px}"
           "</style></head><body><h1>IH</h1><div class=s>");
    s += F("<div class=r><span class=l>Status</span><span class=v id=s>");
    s += runStateName(pwm.getState());
    s += F("</span></div>");
    s += F("<div class=r><span class=l>Freq</span><span class=v id=f>");
    s += String(ctx.params.freq, 0);
    s += F(" Hz</span></div>");
    s += F("<div class=r><span class=l>Duty</span><span class=v id=d>");
    s += String(pwm.getDuty(), 1);
    s += F("%</span></div>");
    s += F("<div class=r><span class=l>DT</span><span class=v id=dt>");
    s += String(ctx.params.deadTimeNs, 0);
    s += F(" ns</span></div>");
    s += F("<div class=r><span class=l>SS</span><span class=v id=ss>");
    s += String(ctx.params.softStartMs);
    s += F(" ms</span></div>");
    s += F("<div class=r><span class=l>IP</span><span class=v id=ip>");
    s += wifi.getIP().toString();
    s += F("</span></div>");
    s += F("<div class=r><span class=l>RSSI</span><span class=v id=r>");
    s += String(WiFi.RSSI());
    s += F(" dBm</span></div>");
    s += F("<div class=r><span class=l>Tank Freq</span><span class=v id=tf>");
    s += String(ct.getTankFrequency(), 0);
    s += F(" Hz</span></div>");
    s += F("<div class=r><span class=l>Tank Amp</span><span class=v id=ta>");
    s += String(ct.getTankAmplitude(), 3);
    s += F("</span></div>");
    s += F("<div class=r><span class=l>PLL State</span><span class=v id=ps>");
    s += _pllStateName(pll.getState());
    s += F("</span></div>");
    s += F("<div class=r><span class=l>PLL Err</span><span class=v id=pe>");
    s += String(pll.getPhaseError(), 3);
    s += F("</span></div>");
    s += F("</div>"
            "<div class=s style=text-align:center>"
           "<a href=/start><button class=bs>Start</button></a> "
           "<a href=/stop><button class=bp>Stop</button></a>"
           "</div>"
           "<div class=s>"
           "<form action=/set method=GET style=display:flex;flex-direction:column;gap:6px>");
    s += F("<div class=fr><lb>Freq (Hz)</lb><input name=f type=number value=");
    s += String(ctx.params.freq, 0);
    s += F("></div>");
    s += F("<div class=fr><lb>DT (ns)</lb><input name=dt type=number value=");
    s += String(ctx.params.deadTimeNs, 0);
    s += F("></div>");
    s += F("<div class=fr><lb>SS (ms)</lb><input name=ss type=number value=");
    s += String(ctx.params.softStartMs);
    s += F("></div>");
    s += F("<div class=sm><button class=ba type=submit>Apply</button></div>"
           "</form></div>"
           "<script>"
           "function p(){fetch('/status').then(r=>r.json()).then(d=>{"
           "document.getElementById('s').innerText=d.state;"
           "document.getElementById('f').innerText=d.freq+' Hz';"
           "document.getElementById('d').innerText=d.duty+'%';"
           "document.getElementById('dt').innerText=d.dt+' ns';"
           "document.getElementById('ss').innerText=d.ss+' ms';"
           "document.getElementById('ip').innerText=d.ip;"
            "document.getElementById('r').innerText=d.rssi+' dBm';"
            "document.getElementById('tf').innerText=d.tankFreq+' Hz';"
            "document.getElementById('ta').innerText=d.tankAmp;"
            "document.getElementById('ps').innerText=d.pllState;"
            "document.getElementById('pe').innerText=d.pllErr})"
           ".catch(function(){})}"
           "setInterval(p,2000);p()"
           "</script>"
           "</body></html>");
    return s;
}
