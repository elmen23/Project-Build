#include "Dashboard.h"

String buildDashboard(PWMManager& pwm, AppContext& ctx, WiFiProvisioning& wifi) {
    String s;
    s.reserve(4096);
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
           ".sc{border:1px solid #333;padding:8px;margin-bottom:16px;background:#000}"
           ".sc-c{display:block;width:100%;max-width:700px;margin:0 auto}"
           ".sc-i{display:flex;gap:16px;font-size:12px;color:#666;margin-top:4px;flex-wrap:wrap;justify-content:center}"
           ".sc-i b{color:#0f0}"
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
           "<div class=sc>"
           "<canvas id=sc class=sc-c width=700 height=200></canvas>"
           "<div class=sc-i>"
           "<span>CH1 <b id=sc-d1>0</b>%</span>"
           "<span>CH2 <b id=sc-d2>0</b>%</span>"
           "<span>Freq <b id=sc-f>0</b>Hz</span>"
           "<span>DT <b id=sc-dt>0</b>ns</span>"
           "<span id=sc-t style=color:#0a0>AUTO</span>"
           "</div></div>"
           "<script>"
           "var cv=document.getElementById('sc'),cx=cv.getContext('2d'),W=cv.width,H=cv.height;"
           "function drawScope(d){"
           "cx.clearRect(0,0,W,H);"
           "cx.strokeStyle='#0a0';cx.lineWidth=0.5;"
           "for(var i=1;i<10;i++){var x=i*W/10;cx.beginPath();cx.moveTo(x,0);cx.lineTo(x,H);cx.stroke()}"
           "for(var i=1;i<10;i++){var y=i*H/10;cx.beginPath();cx.moveTo(0,y);cx.lineTo(W,y);cx.stroke()}"
           "cx.font='10px monospace';cx.fillStyle='#ff0';cx.fillText('CH1',4,H*0.25-4);"
           "cx.fillStyle='#0af';cx.fillText('CH2',4,H*0.75-4);"
           "var run=d.running,freq=d.freq,duty=d.duty,dtNs=d.dt;"
           "if(!run||freq<=0){"
           "cx.fillStyle='#444';cx.font='14px monospace';cx.textAlign='center';"
           "cx.fillText('--- STOPPED ---',W/2,H/2);cx.textAlign='left';"
           "cx.strokeStyle='#040';cx.lineWidth=1;cx.setLineDash([4,4]);"
           "cx.beginPath();cx.moveTo(0,H*0.25);cx.lineTo(W,H*0.25);cx.stroke();"
           "cx.beginPath();cx.moveTo(0,H*0.75);cx.lineTo(W,H*0.75);cx.stroke();"
           "cx.setLineDash([]);return}"
           "var T=1/freq,onT=duty/100*T,dt=dtNs/1e9;"
           "if(dt>T/4)dt=T/4;"
           "var n=3,totalT=T*n;"
           "for(var p=0;p<n;p++){"
           "var x1=(p*T+onT)/totalT*W,x2=(p*T+onT+dt)/totalT*W;"
           "cx.fillStyle='rgba(255,0,0,0.06)';cx.fillRect(x1,0,x2-x1,H);"
           "var x3=(p*T+T-dt)/totalT*W,x4=(p*T+T)/totalT*W;"
           "cx.fillRect(x3,0,x4-x3,H)}"
           "var g1=H*0.25,h1=H*0.25-36;"
           "cx.beginPath();cx.strokeStyle='#ff0';cx.lineWidth=2;"
           "for(var p=0;p<n;p++){"
           "var t0=p*T,x0=t0/totalT*W,x1=(t0+onT)/totalT*W,x2=(t0+T)/totalT*W;"
           "cx.moveTo(x0,h1);cx.lineTo(x1,h1);cx.lineTo(x1,g1);cx.lineTo(x2,g1)}"
           "cx.stroke();"
           "var g2=H*0.75,h2=H*0.75-36;"
           "cx.beginPath();cx.strokeStyle='#0af';cx.lineWidth=2;"
           "for(var p=0;p<n;p++){"
           "var t0=p*T,x0=t0/totalT*W,x1=(t0+onT+dt)/totalT*W,"
           "x2=(t0+T-dt)/totalT*W,x3=(t0+T)/totalT*W;"
           "cx.moveTo(x0,g2);cx.lineTo(x1,g2);cx.lineTo(x1,h2);"
           "cx.lineTo(x2,h2);cx.lineTo(x2,g2);cx.lineTo(x3,g2)}"
           "cx.stroke()}"
           "function p(){"
           "fetch('/status').then(function(r){return r.json()}).then(function(d){"
           "document.getElementById('s').innerText=d.state;"
           "document.getElementById('f').innerText=d.freq+' Hz';"
           "document.getElementById('d').innerText=d.duty+'%';"
           "document.getElementById('dt').innerText=d.dt+' ns';"
           "document.getElementById('ss').innerText=d.ss+' ms';"
           "document.getElementById('ip').innerText=d.ip;"
           "document.getElementById('r').innerText=d.rssi+' dBm';"
           "document.getElementById('sc-d1').innerText=d.duty;"
           "document.getElementById('sc-d2').innerText=(100-d.duty).toFixed(1);"
           "document.getElementById('sc-f').innerText=d.freq;"
           "document.getElementById('sc-dt').innerText=d.dt;"
           "document.getElementById('sc-t').innerText=d.running?'AUTO':'--';"
           "drawScope(d)}).catch(function(){})}"
           "setInterval(p,300);p()"
           "</script>"
           "</body></html>");
    return s;
}
