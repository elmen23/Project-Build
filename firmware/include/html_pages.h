#pragma once
/**
 * ═══════════════════════════════════════════════════════════════════
 *  html_pages.h  —  v3.0 Real-time Edition
 *  Embedded HTML pages for the Induction Heater Controller:
 *   1. SETUP_HTML  → WiFi Provisioning (scan + connect)
 *   2. INDEX_HTML  → Control Panel with WebSocket, Emergency Stop,
 *                     Run Timeout, Event Log, Config Editor
 * ═══════════════════════════════════════════════════════════════════
 */

// ═══════════════════════════════════════════════════════════════════
//  PAGE 1: WiFi Provisioning (Setup Mode)
// ═══════════════════════════════════════════════════════════════════
static const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>إعداد الشبكة — Induction Heater</title>
<style>
  :root {
    --bg: #0d0f14; --surface: #161a24; --border: #252b3a;
    --accent: #f97316; --accent2: #fb923c; --text: #e2e8f0;
    --muted: #64748b; --green: #22c55e; --red: #ef4444;
    --yellow: #eab308; --r: 12px;
  }
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg); color: var(--text);
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    min-height: 100svh; display: flex; flex-direction: column;
    align-items: center; padding: 32px 16px 40px;
  }
  .logo-wrap { text-align: center; margin-bottom: 32px; }
  .logo {
    font-size: 2rem; font-weight: 800;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .sub { color: var(--muted); font-size: .82rem; margin-top: 4px; }
  .card { width: 100%; max-width: 400px; background: var(--surface); border: 1px solid var(--border);
            border-radius: var(--r); padding: 24px; margin-bottom: 14px; }
  .card-title { font-size: .72rem; font-weight: 700; text-transform: uppercase; letter-spacing: 1.2px;
                color: var(--accent); margin-bottom: 18px; }
  .net-list { display: flex; flex-direction: column; gap: 8px; }
  .net-item {
    display: flex; align-items: center; gap: 12px;
    padding: 11px 14px; border-radius: 8px; background: var(--bg);
    border: 1.5px solid var(--border); cursor: pointer;
    transition: border-color .2s, background .2s; user-select: none;
  }
  .net-item:hover    { border-color: var(--accent); background: rgba(249,115,22,.06); }
  .net-item.selected { border-color: var(--accent); background: rgba(249,115,22,.1); }
  .net-ssid { font-size: .9rem; font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .net-meta { font-size: .72rem; color: var(--muted); margin-top: 2px; }
  .rssi-bar { display: flex; gap: 2px; align-items: flex-end; flex-shrink: 0; }
  .rssi-bar span { width: 4px; border-radius: 2px; background: var(--border); transition: background .3s; }
  .rssi-bar span.on { background: var(--accent); }
  input[type="text"], input[type="password"] {
    width: 100%; background: var(--bg); border: 1.5px solid var(--border);
    border-radius: 8px; color: var(--text); font-size: .95rem;
    padding: 10px 42px 10px 14px; outline: none; transition: border-color .2s;
    direction: ltr; text-align: left;
  }
  input:focus { border-color: var(--accent); }
  button.primary {
    flex: 1; padding: 13px; border: none; border-radius: 9px;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    color: #fff; font-size: .95rem; font-weight: 700; cursor: pointer;
  }
  button.secondary {
    padding: 13px 18px; border-radius: 9px; background: var(--surface);
    border: 1px solid var(--border); color: var(--muted);
    font-size: .88rem; font-weight: 600; cursor: pointer;
  }
  .btn-row { display: flex; gap: 10px; margin-top: 4px; }
  .hidden { display: none !important; }
  @keyframes spin { to { transform: rotate(360deg); } }
  .spinner { width: 40px; height: 40px; border-radius: 50%;
             border: 3px solid var(--border); border-top-color: var(--accent);
             animation: spin .8s linear infinite; }
</style>
</head>
<body>
<div class="logo-wrap">
  <div class="logo">⚡ Induction</div>
  <div class="sub">إعداد اتصال الشبكة</div>
</div>
<div class="card">
  <div class="card-title">الشبكات المتاحة</div>
  <div id="net-list" class="net-list"></div>
  <div class="btn-row" style="margin-top:16px">
    <button class="secondary" onclick="startScan()" id="rescanBtn">↻ إعادة البحث</button>
  </div>
</div>
<script>
async function startScan() {
  document.getElementById('net-list').innerHTML = '<div style="display:flex;justify-content:center;padding:20px"><div class="spinner"></div></div>';
  try {
    const r = await fetch('/scan');
    const nets = await r.json();
    renderNetworks(nets);
  } catch(e) {
    document.getElementById('net-list').innerHTML = '<div style="color:var(--red);font-size:.85rem;text-align:center">فشل البحث</div>';
  }
}
function renderNetworks(nets) {
  const list = document.getElementById('net-list');
  if (!nets || nets.length === 0) { list.innerHTML = '<div style="color:var(--muted);font-size:.85rem;text-align:center">لم يتم العثور على شبكات</div>'; return; }
  list.innerHTML = nets.map(n => `
    <div class="net-item" onclick="connect('${n.ssid.replace(/'/g,"\\'")}',${n.open},${n.ch})">
      <div style="flex:1"><div class="net-ssid">${n.ssid}</div><div class="net-meta">Ch ${n.ch||'?'} · ${n.rssi} dBm</div></div>
      <div class="rssi-bar">${[4,7,10,14].map((h,i)=>`<span style="height:${h}px" class="${i<(n.rssi>=-50?4:n.rssi>=-65?3:n.rssi>=-75?2:n.rssi>=-85?1:0)?'on':''}"></span>`).join('')}</div>
      ${n.open?'':'🔒'}
    </div>`).join('');
}
function connect(ssid,open,ch) {
  const pass = open ? '' : prompt('كلمة المرور لـ ' + ssid);
  if (pass===null) return;
  fetch('/connect',{method:'POST',body:new URLSearchParams({ssid,pass,ch})});
  alert('تم إرسال طلب الاتصال. انتظر قليلاً...');
  setTimeout(()=>location.href='/',3000);
}
startScan();
</script>
</body>
</html>
)rawliteral";


// ═══════════════════════════════════════════════════════════════════
//  PAGE 2: Control Panel (WebSocket Real-time)
// ═══════════════════════════════════════════════════════════════════
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Induction Control Panel v3.0</title>
<style>
  :root {
    --bg: #0d0f14; --surface: #161a24; --border: #252b3a;
    --accent: #f97316; --accent2: #fb923c; --text: #e2e8f0;
    --muted: #64748b; --green: #22c55e; --red: #ef4444;
    --yellow: #eab308; --r: 12px;
  }
  *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg); color: var(--text);
    font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
    min-height: 100vh; display: flex; flex-direction: column;
    align-items: center; padding: 24px 16px 40px;
  }
  header { text-align: center; margin-bottom: 20px; }
  .logo { font-size: 1.8rem; font-weight: 800;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
  .sub { color: var(--muted); font-size: .78rem; margin-top: 2px; }

  /* Connection dot */
  .ws-dot { width: 8px; height: 8px; border-radius: 50%; display: inline-block; margin-left: 6px; }
  .ws-dot.on { background: var(--green); animation: pulse 2s infinite; }
  .ws-dot.off { background: var(--red); }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }

  /* Status Bar */
  .status-bar {
    display: flex; align-items: center; gap: 8px; flex-wrap: wrap;
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 999px; padding: 6px 16px; margin-bottom: 16px;
    font-size: .75rem; max-width: 500px; width: 100%;
  }
  .dot { width: 8px; height: 8px; border-radius: 50%; }
  .dot.green { background: var(--green); animation: pulse 2s infinite; }
  .dot.yellow{ background: var(--yellow); animation: pulse 1.2s infinite; }
  .dot.red   { background: var(--red); }
  .sep { color: var(--border); }

  /* Tabs */
  .tabs { display: flex; gap: 4px; width: 100%; max-width: 500px; margin-bottom: 14px; }
  .tab { flex:1; padding: 10px; border: none; border-radius: 8px; background: var(--surface);
         color: var(--muted); font-size: .82rem; font-weight: 700; cursor: pointer; }
  .tab.active { background: var(--accent); color: #fff; }

  /* Cards */
  .card { width: 100%; max-width: 500px; background: var(--surface);
          border: 1px solid var(--border); border-radius: var(--r);
          padding: 20px; margin-bottom: 12px; }
  .card-title { font-size: .72rem; font-weight: 700; text-transform: uppercase;
                letter-spacing: 1.2px; color: var(--accent); margin-bottom: 16px; }

  /* Live Grid */
  .live-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  .live-item { background: var(--bg); border: 1px solid var(--border);
               border-radius: 8px; padding: 12px 14px; }
  .lv-label { font-size: .68rem; color: var(--muted); margin-bottom: 4px; }
  .lv-val { font-size: 1.05rem; font-weight: 700; font-variant-numeric: tabular-nums; }

  /* Run Banner */
  .run-banner {
    width: 100%; max-width: 500px; border-radius: var(--r); padding: 14px 18px;
    margin-bottom: 12px; display: flex; align-items: center; justify-content: space-between;
    font-size: .9rem; font-weight: 700; transition: all .3s;
  }
  .run-banner.idle  { background: rgba(100,116,139,.12); color: var(--muted); border: 1px solid var(--border); }
  .run-banner.run   { background: rgba(34,197,94,.12); color: var(--green); border: 1px solid rgba(34,197,94,.3); }
  .run-banner.start { background: rgba(234,179,8,.12); color: var(--yellow); border: 1px solid rgba(234,179,8,.3); }
  .run-banner.emergency { background: rgba(239,68,68,.15); color: var(--red); border: 1px solid rgba(239,68,68,.4); }

  /* Control Buttons */
  .ctrl-row { display: flex; gap: 10px; width: 100%; max-width: 500px; margin-bottom: 12px; }
  .ctrl-btn {
    flex: 1; padding: 16px; border: none; border-radius: var(--r); font-size: 1rem;
    font-weight: 800; cursor: pointer; transition: all .2s; display: flex;
    align-items: center; justify-content: center; gap: 8px; letter-spacing: .5px;
  }
  .ctrl-btn:hover { transform: translateY(-1px); }
  .ctrl-btn:active:not(:disabled) { transform: scale(.97); }
  .ctrl-btn:disabled { opacity: .5; cursor: not-allowed; transform: none !important; }
  .btn-start { background: linear-gradient(135deg, var(--green), #16a34a); color: #fff;
               box-shadow: 0 4px 16px rgba(34,197,94,.25); }
  .btn-stop  { background: linear-gradient(135deg, var(--red), #dc2626); color: #fff;
               box-shadow: 0 4px 16px rgba(239,68,68,.25); }
  .btn-emergency { background: linear-gradient(135deg, #7f1d1d, var(--red)); color: #fff;
                   box-shadow: 0 4px 16px rgba(239,68,68,.35); border: 2px solid rgba(239,68,68,.5); }

  /* Timer display */
  .timer-box { font-family: ui-monospace, monospace; font-size: 1.3rem; font-weight: 700;
               color: var(--yellow); background: var(--bg); border: 1px solid var(--border);
               border-radius: 8px; padding: 8px 16px; text-align: center; margin-bottom: 12px; }

  /* Fields */
  .field { margin-bottom: 16px; }
  .field:last-child { margin-bottom: 0; }
  .fh { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 6px; }
  label { font-size: .82rem; color: var(--muted); font-weight: 500; }
  .badge { font-size: .9rem; font-weight: 700; }
  input[type="number"], input[type="text"] {
    width: 100%; background: var(--bg); border: 1.5px solid var(--border);
    border-radius: 8px; color: var(--text); font-size: .92rem;
    padding: 10px 14px; outline: none; transition: border-color .2s; direction: ltr;
  }
  input:focus { border-color: var(--accent); }
  input[type="range"] {
    -webkit-appearance: none; width: 100%; height: 6px;
    background: var(--border); border-radius: 3px; outline: none;
  }
  input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%;
    background: var(--accent); cursor: pointer;
    box-shadow: 0 0 0 3px rgba(249,115,22,.25);
  }
  .errmsg { font-size: .72rem; color: var(--red); margin-top: 4px; display: none; }

  /* Buttons */
  .btn-row { display: flex; gap: 10px; width: 100%; max-width: 500px; }
  button.primary {
    flex: 1; padding: 12px; border: none; border-radius: 9px;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    color: #fff; font-size: .9rem; font-weight: 700; cursor: pointer;
  }
  button.secondary {
    padding: 12px 16px; border-radius: 9px; background: var(--surface);
    border: 1px solid var(--border); color: var(--muted);
    font-size: .85rem; font-weight: 600; cursor: pointer;
  }
  button:disabled { opacity: .4; cursor: not-allowed; }

  /* Event Log */
  .log-list { max-height: 300px; overflow-y: auto; display: flex; flex-direction: column; gap: 6px; }
  .log-item { background: var(--bg); border: 1px solid var(--border); border-radius: 6px;
              padding: 8px 12px; font-size: .78rem; }
  .log-ts { color: var(--muted); font-size: .7rem; }
  .log-type { font-weight: 700; font-size: .72rem; padding: 1px 6px; border-radius: 4px; display: inline-block; margin-left: 6px; }
  .log-type.START { background: rgba(34,197,94,.15); color: var(--green); }
  .log-type.STOP { background: rgba(100,116,139,.15); color: var(--muted); }
  .log-type.EMERGENCY { background: rgba(239,68,68,.2); color: var(--red); }
  .log-type.TIMEOUT { background: rgba(234,179,8,.15); color: var(--yellow); }
  .log-type.ERROR { background: rgba(239,68,68,.15); color: var(--red); }
  .log-type.INFO { background: rgba(59,130,246,.15); color: #60a5fa; }

  /* Config Editor */
  textarea.config-editor {
    width: 100%; min-height: 220px; background: var(--bg); border: 1.5px solid var(--border);
    border-radius: 8px; color: var(--text); font-family: ui-monospace, monospace;
    font-size: .82rem; padding: 12px; outline: none; resize: vertical; direction: ltr;
  }
  textarea.config-editor:focus { border-color: var(--accent); }

  /* Safety Warning */
  .safety-note {
    width: 100%; max-width: 500px; background: rgba(239,68,68,.06);
    border: 1px solid rgba(239,68,68,.2); border-radius: 8px;
    padding: 10px 14px; font-size: .75rem; color: var(--red);
    margin-bottom: 12px; text-align: center;
  }

  /* Toast */
  .toast {
    position: fixed; bottom: 24px; left: 50%;
    transform: translateX(-50%) translateY(80px);
    background: #1e2433; border: 1px solid var(--border);
    border-radius: 10px; padding: 12px 24px;
    font-size: .88rem; font-weight: 600;
    transition: transform .35s cubic-bezier(.34,1.56,.64,1), opacity .3s;
    opacity: 0; pointer-events: none; white-space: nowrap; z-index: 100;
  }
  .toast.show { transform: translateX(-50%) translateY(0); opacity: 1; }

  .hidden { display: none !important; }
</style>
</head>
<body>

<header>
  <div class="logo">⚡ Induction Control <span class="ws-dot off" id="wsDot"></span></div>
  <div class="sub">ESP32 MCPWM v3.0 — WebSocket Real-time</div>
</header>

<!-- Status Bar -->
<div class="status-bar">
  <div class="dot green" id="status-dot"></div>
  <span id="netInfo" style="color:var(--muted)">جارٍ الاتصال…</span>
  <span class="sep">|</span>
  <span id="runState" style="font-weight:700">—</span>
</div>

<!-- Timer -->
<div class="timer-box hidden" id="timerBox">⏱ <span id="timerVal">00:00:00</span></div>

<!-- Safety Warning -->
<div class="safety-note">
  ⚠️ تحذير: تأكد من توصيل المبرد وعدم تشغيل الجهاز بدون حمل!
</div>

<!-- Run Banner -->
<div class="run-banner idle" id="runBanner">
  <span id="runBannerText">الجهاز جاهز — اضغط ▶ لتشغيل</span>
  <span id="runBannerIcon">⚪</span>
</div>

<!-- Emergency Stop -->
<div class="ctrl-row">
  <button class="ctrl-btn btn-emergency" id="btnEmergency" onclick="doEmergency()">🛑 إيقاف طوارئ</button>
</div>

<!-- Start / Stop Controls -->
<div class="ctrl-row">
  <button class="ctrl-btn btn-start" id="btnStart" onclick="doStart()">▶ تشغيل</button>
  <button class="ctrl-btn btn-stop"  id="btnStop"  onclick="doStop()">■ إيقاف</button>
</div>

<!-- Tabs -->
<div class="tabs">
  <button class="tab active" onclick="showTab('control')" id="tab-control">التحكم</button>
  <button class="tab" onclick="showTab('events')" id="tab-events">السجل</button>
  <button class="tab" onclick="showTab('config')" id="tab-config">الإعدادات</button>
</div>

<!-- ═══ TAB: Control ═══ -->
<div id="panel-control">
  <!-- Live Values -->
  <div class="card">
    <div class="card-title">القيم الحية</div>
    <div class="live-grid">
      <div class="live-item"><div class="lv-label">التردد</div><div class="lv-val"><span id="lv-f">—</span> <small style="color:var(--muted);font-size:.65rem">Hz</small></div></div>
      <div class="live-item"><div class="lv-label">نسبة الإشغال</div><div class="lv-val"><span id="lv-d">—</span> <small style="color:var(--muted);font-size:.65rem">%</small></div></div>
      <div class="live-item"><div class="lv-label">Dead Time</div><div class="lv-val"><span id="lv-dt">—</span> <small style="color:var(--muted);font-size:.65rem">ns</small></div></div>
      <div class="live-item"><div class="lv-label">Soft Start</div><div class="lv-val"><span id="lv-ss">—</span> <small style="color:var(--muted);font-size:.65rem">ms</small></div></div>
    </div>
  </div>

  <!-- Settings -->
  <div class="card">
    <div class="card-title">الإعدادات</div>
    <div class="field">
      <div class="fh"><label>التردد</label><span class="badge"><span id="d-f">100000</span><span style="font-size:.65rem;color:var(--muted)">Hz</span></span></div>
      <input id="i-f" type="range" min="1000" max="400000" step="1000" value="100000" oninput="sync('f')">
    </div>
    <div class="field">
      <div class="fh"><label>نسبة الإشغال (Duty)</label><span class="badge"><span id="d-d">50</span><span style="font-size:.65rem;color:var(--muted)">%</span></span></div>
      <input id="i-d" type="number" value="50" min="0" max="95" step="0.5" oninput="sync('d')" onchange="valid('d')">
      <div class="errmsg" id="e-d">يجب أن يكون بين 0 و 95%</div>
    </div>
    <div class="field">
      <div class="fh"><label>Dead Time</label><span class="badge"><span id="d-dt">500</span><span style="font-size:.65rem;color:var(--muted)">ns</span></span></div>
      <input id="i-dt" type="number" value="500" min="0" max="5000" step="100" oninput="sync('dt')" onchange="valid('dt')">
      <div class="errmsg" id="e-dt">يجب أن يكون بين 0 و 5,000 ns</div>
    </div>
    <div class="field">
      <div class="fh"><label>Soft Start</label><span class="badge"><span id="d-ss">3000</span><span style="font-size:.65rem;color:var(--muted)">ms</span></span></div>
      <input id="i-ss" type="number" value="3000" min="500" max="10000" step="100" oninput="sync('ss')" onchange="valid('ss')">
      <div class="errmsg" id="e-ss">يجب أن يكون بين 500 و 10,000 ms</div>
    </div>
    <div class="field">
      <div class="fh"><label>مدة التشغيل الأقصى (Timeout)</label><span class="badge"><span id="d-to">1800</span><span style="font-size:.65rem;color:var(--muted)">sec</span></span></div>
      <input id="i-to" type="number" value="1800" min="60" max="7200" step="60" oninput="sync('to')" onchange="valid('to')">
      <div class="errmsg" id="e-to">يجب أن يكون بين 60 و 7200 ثانية</div>
    </div>
  </div>

  <div class="btn-row">
    <button class="primary" id="applyBtn" onclick="applyAll()">💾 حفظ وتطبيق</button>
    <button class="secondary" onclick="fetchStatus()">🔄 تحديث</button>
  </div>
</div>

<!-- ═══ TAB: Events ═══ -->
<div id="panel-events" class="hidden">
  <div class="card">
    <div class="card-title" style="display:flex;justify-content:space-between;align-items:center">
      <span>سجل الأحداث</span>
      <button class="secondary" style="padding:6px 10px;font-size:.72rem" onclick="clearEvents()">🗑 مسح</button>
    </div>
    <div class="log-list" id="logList"><div style="color:var(--muted);text-align:center;padding:20px">جارٍ التحميل…</div></div>
  </div>
</div>

<!-- ═══ TAB: Config ═══ -->
<div id="panel-config" class="hidden">
  <div class="card">
    <div class="card-title">ملف الإعدادات (JSON)</div>
    <textarea class="config-editor" id="cfgEditor" spellcheck="false"></textarea>
    <div class="btn-row" style="margin-top:12px">
      <button class="primary" onclick="saveConfig()">💾 حفظ الإعدادات</button>
      <button class="secondary" onclick="resetConfig()">↺ استعادة افتراضي</button>
    </div>
  </div>
</div>

<div style="text-align:center;margin-top:12px">
  <button class="secondary" style="font-size:.75rem;padding:8px 14px" onclick="resetWiFi()">🔧 إعادة إعداد الشبكة</button>
</div>

<div class="toast" id="toast"></div>

<script>
const LIMITS = { f:[1000,400000], d:[0,95], dt:[0,5000], ss:[500,10000], to:[60,7200] };
let ws = null;
let wsConnected = false;

function sync(k)  { document.getElementById('d-'+k).textContent = document.getElementById('i-'+k).value; }
function valid(k) {
  if (!LIMITS[k]) return true;
  const inp = document.getElementById('i-'+k);
  const err = document.getElementById('e-'+k);
  const v = parseFloat(inp.value);
  const bad = isNaN(v) || v < LIMITS[k][0] || v > LIMITS[k][1];
  inp.classList.toggle('err', bad);
  err.style.display = bad ? 'block' : 'none';
  return !bad;
}
function validAll() { return ['f','d','dt','ss','to'].every(k => valid(k)); }

let tTimer;
function toast(msg, type='ok') {
  const t = document.getElementById('toast');
  t.textContent = msg; t.className = 'toast show';
  if(type==='bad') t.style.borderColor='var(--red)';
  else if(type==='warn') t.style.borderColor='var(--yellow)';
  else t.style.borderColor='var(--green)';
  clearTimeout(tTimer);
  tTimer = setTimeout(() => t.className = 'toast', 2800);
}

// ── WebSocket ──
function connectWS() {
  const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
  ws = new WebSocket(proto + '//' + location.host + '/ws');
  ws.onopen = () => {
    wsConnected = true;
    document.getElementById('wsDot').className = 'ws-dot on';
    toast('✓ متصل بالوقت الحقيقي');
  };
  ws.onclose = () => {
    wsConnected = false;
    document.getElementById('wsDot').className = 'ws-dot off';
    setTimeout(connectWS, 3000);
  };
  ws.onmessage = (e) => {
    try { updateUI(JSON.parse(e.data)); } catch(_) {}
  };
  ws.onerror = () => { wsConnected = false; };
}

// ── Tab Switching ──
function showTab(name) {
  ['control','events','config'].forEach(t => {
    document.getElementById('panel-'+t).classList.toggle('hidden', t !== name);
    document.getElementById('tab-'+t).classList.toggle('active', t === name);
  });
  if (name === 'events') loadEvents();
  if (name === 'config') loadConfig();
}

// ── UI Update ──
function updateUI(d) {
  document.getElementById('lv-f').textContent  = Number(d.frequency).toLocaleString();
  document.getElementById('lv-d').textContent  = parseFloat(d.duty).toFixed(1);
  document.getElementById('lv-dt').textContent = parseFloat(d.deadTime).toFixed(0);
  document.getElementById('lv-ss').textContent = parseFloat(d.softStartMs).toFixed(0);

  document.getElementById('d-f').textContent  = Number(d.frequency).toLocaleString();
  document.getElementById('d-d').textContent  = parseFloat(d.duty).toFixed(1);
  document.getElementById('d-dt').textContent = parseFloat(d.deadTime).toFixed(0);
  document.getElementById('d-ss').textContent = parseFloat(d.softStartMs).toFixed(0);
  document.getElementById('d-to').textContent = d.runTimeoutSec || 1800;

  if (!document.getElementById('i-f').matches(':focus'))  document.getElementById('i-f').value  = Number(d.frequency);
  if (!document.getElementById('i-d').matches(':focus'))  document.getElementById('i-d').value  = parseFloat(d.duty).toFixed(1);
  if (!document.getElementById('i-dt').matches(':focus')) document.getElementById('i-dt').value = parseFloat(d.deadTime).toFixed(0);
  if (!document.getElementById('i-ss').matches(':focus')) document.getElementById('i-ss').value = parseFloat(d.softStartMs).toFixed(0);
  if (!document.getElementById('i-to').matches(':focus')) document.getElementById('i-to').value = d.runTimeoutSec || 1800;

  // Run state
  const banner = document.getElementById('runBanner');
  const bannerText = document.getElementById('runBannerText');
  const bannerIcon = document.getElementById('runBannerIcon');
  const btnStart = document.getElementById('btnStart');
  const btnStop  = document.getElementById('btnStop');
  const dot      = document.getElementById('status-dot');
  const timerBox = document.getElementById('timerBox');

  if (d.softStarting) {
    banner.className = 'run-banner start';
    bannerText.textContent = 'جارٍ التشغيل التدريجي…';
    bannerIcon.textContent = '🔄';
    btnStart.disabled = true; btnStop.disabled = false;
    dot.className = 'dot yellow'; timerBox.classList.remove('hidden');
  } else if (d.running) {
    banner.className = 'run-banner run';
    bannerText.textContent = 'الجهاز يعمل — اضغط ■ للإيقاف';
    bannerIcon.textContent = '🟢';
    btnStart.disabled = true; btnStop.disabled = false;
    dot.className = 'dot green'; timerBox.classList.remove('hidden');
    // Update timer
    const secs = Math.floor(d.runTimeMs / 1000);
    const h = String(Math.floor(secs/3600)).padStart(2,'0');
    const m = String(Math.floor((secs%3600)/60)).padStart(2,'0');
    const s = String(secs%60).padStart(2,'0');
    document.getElementById('timerVal').textContent = `${h}:${m}:${s}`;
  } else {
    banner.className = 'run-banner idle';
    bannerText.textContent = 'الجهاز جاهز — اضغط ▶ لتشغيل';
    bannerIcon.textContent = '⚪';
    btnStart.disabled = false; btnStop.disabled = true;
    dot.className = 'dot green'; timerBox.classList.add('hidden');
  }

  document.getElementById('runState').textContent =
    d.softStarting ? '🔄 Soft Start…' : d.running ? '🟢 يعمل' : '⚪ جاهز';

  document.getElementById('netInfo').textContent =
    `📶 ${d.ssid || '—'} · ${d.ip || '—'} · ${d.rssi || 0} dBm`;
}

// ── Actions ──
async function doStart() {
  if (wsConnected) { ws.send('start'); toast('▶ تم التشغيل'); }
  else {
    try { await fetch('/start'); toast('▶ تم التشغيل'); }
    catch(e) { toast('فشل التشغيل', 'bad'); }
  }
}
async function doStop() {
  if (wsConnected) { ws.send('stop'); toast('■ تم الإيقاف'); }
  else {
    try { await fetch('/stop'); toast('■ تم الإيقاف'); }
    catch(e) { toast('فشل الإيقاف', 'bad'); }
  }
}
async function doEmergency() {
  if (!confirm('🛑 هل أنت متأكد من إيقاف الطوارئ؟\nسيتم إيقاف PWM فوراً.')) return;
  try {
    await fetch('/emergency', {method:'POST'});
    toast('🛑 إيقاف طوارئ!', 'bad');
    updateRunUI('idle');
  } catch(e) { toast('فشل', 'bad'); }
}

function updateRunUI(state) {
  const banner = document.getElementById('runBanner');
  if (state === 'idle') {
    banner.className = 'run-banner idle';
    document.getElementById('runBannerText').textContent = 'الجهاز جاهز';
    document.getElementById('btnStart').disabled = false;
    document.getElementById('btnStop').disabled = true;
    document.getElementById('timerBox').classList.add('hidden');
  }
}

async function applyAll() {
  if (!validAll()) { toast('أصلح الأخطاء أولاً', 'bad'); return; }
  const btn = document.getElementById('applyBtn');
  btn.disabled = true; btn.textContent = 'جارٍ الحفظ…';
  try {
    const p = new URLSearchParams({
      f:  document.getElementById('i-f').value,
      d:  document.getElementById('i-d').value,
      dt: document.getElementById('i-dt').value,
      ss: document.getElementById('i-ss').value,
    });
    const r = await fetch('/set?' + p);
    if (!r.ok) throw new Error();
    toast('✓ تم الحفظ');
  } catch(e) { toast('خطأ في الحفظ', 'bad'); }
  finally { btn.disabled = false; btn.textContent = '💾 حفظ وتطبيق'; }
}

async function fetchStatus() {
  try {
    const r = await fetch('/status');
    const d = await r.json();
    updateUI(d);
  } catch(e) {
    document.getElementById('netInfo').textContent = '🔴 انقطع الاتصال';
    document.getElementById('status-dot').className = 'dot red';
  }
}

// ── Events ──
async function loadEvents() {
  const list = document.getElementById('logList');
  list.innerHTML = '<div style="color:var(--muted);text-align:center;padding:20px">جارٍ التحميل…</div>';
  try {
    const r = await fetch('/events');
    const events = await r.json();
    if (!events.length) { list.innerHTML = '<div style="color:var(--muted);text-align:center;padding:20px">لا توجد أحداث</div>'; return; }
    list.innerHTML = events.reverse().map(e => {
      const sec = Math.floor(e.ts/1000);
      const h=String(Math.floor(sec/3600)).padStart(2,'0');
      const m=String(Math.floor((sec%3600)/60)).padStart(2,'0');
      const s=String(sec%60).padStart(2,'0');
      return `<div class="log-item"><span class="log-ts">${h}:${m}:${s}</span><span class="log-type ${e.type}">${e.type}</span><div style="margin-top:4px">${e.msg}</div></div>`;
    }).join('');
  } catch(e) { list.innerHTML = '<div style="color:var(--red);text-align:center">فشل التحميل</div>'; }
}
async function clearEvents() {
  if (!confirm('مسح جميع الأحداث؟')) return;
  await fetch('/events/clear', {method:'POST'});
  loadEvents();
}

// ── Config ──
async function loadConfig() {
  try {
    const r = await fetch('/config');
    const cfg = await r.json();
    document.getElementById('cfgEditor').value = JSON.stringify(cfg, null, 2);
  } catch(e) { document.getElementById('cfgEditor').value = '{\n  "error": "Failed to load"\n}'; }
}
async function saveConfig() {
  const raw = document.getElementById('cfgEditor').value;
  try { JSON.parse(raw); } catch(e) { toast('JSON غير صالح!', 'bad'); return; }
  try {
    const r = await fetch('/config', {method:'POST', body:raw, headers:{'Content-Type':'application/json'}});
    if (!r.ok) throw new Error();
    toast('✓ تم حفظ الإعدادات');
  } catch(e) { toast('فشل الحفظ', 'bad'); }
}
async function resetConfig() {
  if (!confirm('استعادة الإعدادات الافتراضية؟')) return;
  await fetch('/config/reset', {method:'POST'});
  loadConfig();
  toast('تمت الاستعادة');
}

// ── Reset WiFi ──
async function resetWiFi() {
  if (!confirm('سيتم مسح بيانات الشبكة وإعادة التشغيل. هل أنت متأكد؟')) return;
  await fetch('/reset-wifi', {method:'POST'});
  alert('جارٍ إعادة التشغيل…');
}

// Boot
connectWS();
fetchStatus();
setInterval(() => { if (!wsConnected) fetchStatus(); }, 3000);
</script>
</body>
</html>
)rawliteral";
