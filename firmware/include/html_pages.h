#pragma once
/**
 * ═══════════════════════════════════════════════════════════════════
 *  html_pages.h  —  v2.1 Enhanced
 *  Embedded HTML pages for the Induction Heater Controller:
 *   1. SETUP_HTML  → WiFi Provisioning (scan + connect)
 *   2. INDEX_HTML  → Control Panel (frequency, duty, dead-time, soft-start)
 *
 *  Enhancements over v2.0:
 *   • Dedicated Start/Stop controls in the UI
 *   • Real-time status indicators with color coding
 *   • Improved input validation with visual feedback
 *   • Better responsive layout for mobile devices
 *   • Connection quality indicator (RSSI visualization)
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

  /* Logo */
  .logo-wrap { text-align: center; margin-bottom: 32px; }
  .logo {
    font-size: 2rem; font-weight: 800;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .sub { color: var(--muted); font-size: .82rem; margin-top: 4px; }

  /* Steps */
  .steps { display: flex; gap: 0; align-items: center; width: 100%; max-width: 400px; margin-bottom: 28px; }
  .step { flex: 1; display: flex; flex-direction: column; align-items: center; gap: 6px; position: relative; }
  .step:not(:last-child)::after {
    content: ''; position: absolute; top: 14px; left: calc(50% + 14px);
    width: calc(100% - 28px); height: 2px; background: var(--border); transition: background .4s;
  }
  .step.done:not(:last-child)::after { background: var(--accent); }
  .step-icon {
    width: 28px; height: 28px; border-radius: 50%; border: 2px solid var(--border);
    display: flex; align-items: center; justify-content: center;
    font-size: .75rem; font-weight: 700; background: var(--bg); color: var(--muted);
    transition: all .3s; z-index: 1;
  }
  .step.active .step-icon { border-color: var(--accent); color: var(--accent); }
  .step.done   .step-icon { border-color: var(--accent); background: var(--accent); color: #fff; }
  .step-label { font-size: .68rem; color: var(--muted); }
  .step.active .step-label { color: var(--accent); }
  .step.done   .step-label { color: var(--text); }

  /* Card */
  .card { width: 100%; max-width: 400px; background: var(--surface); border: 1px solid var(--border);
          border-radius: var(--r); padding: 24px; margin-bottom: 14px; }
  .card-title { font-size: .72rem; font-weight: 700; text-transform: uppercase; letter-spacing: 1.2px;
                color: var(--accent); margin-bottom: 18px; }

  /* Network List */
  .net-list { display: flex; flex-direction: column; gap: 8px; }
  .net-item {
    display: flex; align-items: center; gap: 12px;
    padding: 11px 14px; border-radius: 8px; background: var(--bg);
    border: 1.5px solid var(--border); cursor: pointer;
    transition: border-color .2s, background .2s; user-select: none;
  }
  .net-item:hover    { border-color: var(--accent); background: rgba(249,115,22,.06); }
  .net-item.selected { border-color: var(--accent); background: rgba(249,115,22,.1); }
  .net-icon { font-size: 1.1rem; flex-shrink: 0; }
  .net-info { flex: 1; min-width: 0; }
  .net-ssid { font-size: .9rem; font-weight: 600; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
  .net-meta { font-size: .72rem; color: var(--muted); margin-top: 2px; }
  .net-lock { font-size: .8rem; color: var(--muted); flex-shrink: 0; }
  .rssi-bar { display: flex; gap: 2px; align-items: flex-end; flex-shrink: 0; }
  .rssi-bar span { width: 4px; border-radius: 2px; background: var(--border); transition: background .3s; }
  .rssi-bar span.on { background: var(--accent); }

  /* Inputs */
  .field { margin-bottom: 16px; }
  .field:last-child { margin-bottom: 0; }
  label { display: block; font-size: .82rem; color: var(--muted); margin-bottom: 6px; }
  .inp-wrap { position: relative; }
  input[type="text"], input[type="password"] {
    width: 100%; background: var(--bg); border: 1.5px solid var(--border);
    border-radius: 8px; color: var(--text); font-size: .95rem;
    padding: 10px 42px 10px 14px; outline: none; transition: border-color .2s;
    direction: ltr; text-align: left;
  }
  input:focus { border-color: var(--accent); }
  input.error { border-color: var(--red) !important; }
  .eye-btn { position: absolute; left: 12px; top: 50%; transform: translateY(-50%); background: none;
             border: none; cursor: pointer; color: var(--muted); font-size: 1rem; padding: 4px; }

  /* Buttons */
  .btn-row { display: flex; gap: 10px; margin-top: 4px; }
  button.primary {
    flex: 1; padding: 13px; border: none; border-radius: 9px;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    color: #fff; font-size: .95rem; font-weight: 700;
    cursor: pointer; transition: opacity .2s, transform .1s;
  }
  button.secondary {
    padding: 13px 18px; border-radius: 9px; background: var(--surface);
    border: 1px solid var(--border); color: var(--muted);
    font-size: .88rem; font-weight: 600; cursor: pointer; transition: opacity .2s;
  }
  button:disabled { opacity: .4; cursor: not-allowed; }
  button:active:not(:disabled) { transform: scale(.97); }

  /* Status Panel */
  .status-panel { display: flex; flex-direction: column; align-items: center; gap: 16px; padding: 8px 0; }
  .big-icon { font-size: 3rem; line-height: 1; }
  .status-title { font-size: 1.1rem; font-weight: 700; }
  .status-sub { font-size: .84rem; color: var(--muted); text-align: center; }
  .progress-ring { position: relative; width: 72px; height: 72px; }
  .progress-ring svg { transform: rotate(-90deg); }
  .ring-bg { fill: none; stroke: var(--border); stroke-width: 4; }
  .ring-fill { fill: none; stroke: var(--accent); stroke-width: 4; stroke-linecap: round;
               stroke-dasharray: 201; stroke-dashoffset: 201; transition: stroke-dashoffset .5s linear; }
  .ring-label { position: absolute; top: 50%; left: 50%; transform: translate(-50%,-50%);
                font-size: .75rem; font-weight: 700; color: var(--text); }
  .ip-box { background: var(--bg); border: 1px solid var(--border); border-radius: 8px;
            padding: 10px 18px; font-family: ui-monospace, monospace;
            font-size: 1.05rem; color: var(--accent); letter-spacing: 1px; }
  .tag { display: inline-flex; align-items: center; gap: 4px;
         padding: 3px 10px; border-radius: 999px; font-size: .72rem; font-weight: 700; }
  .tag.green { background: rgba(34,197,94,.15); color: var(--green); }
  .tag.red   { background: rgba(239,68,68,.15);  color: var(--red); }
  .tag.yellow{ background: rgba(234,179,8,.15);  color: var(--yellow); }

  /* Animations */
  @keyframes spin { to { transform: rotate(360deg); } }
  .spinner { width: 40px; height: 40px; border-radius: 50%;
             border: 3px solid var(--border); border-top-color: var(--accent);
             animation: spin .8s linear infinite; }
  @keyframes blink { 0%,100%{opacity:.2} 50%{opacity:1} }
  .scan-row { display: flex; align-items: center; gap: 10px; color: var(--muted); font-size: .88rem; }
  .scan-dot { display: inline-block; width: 6px; height: 6px; border-radius: 50%;
              background: var(--accent); animation: blink 1s ease-in-out infinite; }
  .scan-dot:nth-child(2) { animation-delay: .2s; }
  .scan-dot:nth-child(3) { animation-delay: .4s; }

  .hidden { display: none !important; }
</style>
</head>
<body>

<!-- Logo -->
<div class="logo-wrap">
  <div class="logo">⚡ Induction</div>
  <div class="sub">إعداد اتصال الشبكة</div>
</div>

<!-- Steps -->
<div class="steps" id="steps">
  <div class="step active" id="s1"><div class="step-icon">1</div><div class="step-label">الشبكات</div></div>
  <div class="step" id="s2"><div class="step-icon">2</div><div class="step-label">الاتصال</div></div>
  <div class="step" id="s3"><div class="step-icon">✓</div><div class="step-label">تم</div></div>
</div>

<!-- ═══ PHASE 1: Scan ═══ -->
<div id="phase-scan" class="card">
  <div class="card-title">الشبكات المتاحة</div>
  <div id="scan-loader" class="scan-row" style="margin-bottom:14px">
    <span class="scan-dot"></span><span class="scan-dot"></span><span class="scan-dot"></span>
    <span>جارٍ البحث عن الشبكات…</span>
  </div>
  <div id="net-list" class="net-list"></div>
  <div class="btn-row" style="margin-top:16px">
    <button class="secondary" onclick="startScan()" id="rescanBtn" disabled>↻ إعادة البحث</button>
  </div>
  <div style="text-align:center;margin-top:14px;padding-top:12px;border-top:1px solid var(--border)">
    <button class="primary" onclick="skipToAP()" style="background:linear-gradient(135deg,var(--muted),#475569);max-width:260px">
      ⚡ استخدام الجهاز في وضع AP
    </button>
    <div style="font-size:.72rem;color:var(--muted);margin-top:6px">تجاوز اتصال WiFi واستخدم لوحة التحكم عبر شبكة الجهاز</div>
  </div>
</div>

<!-- ═══ PHASE 2: Password ═══ -->
<div id="phase-pass" class="card hidden">
  <div class="card-title">إدخال كلمة المرور</div>
  <div class="field">
    <label>الشبكة المختارة</label>
    <div style="font-weight:700;font-size:1rem;color:var(--accent);padding:4px 0" id="chosen-ssid-lbl">—</div>
  </div>
  <div class="field" id="pass-field">
    <label for="inp-pass">كلمة المرور</label>
    <div class="inp-wrap">
      <input type="password" id="inp-pass" placeholder="أدخل كلمة المرور"
             autocomplete="off" autocorrect="off" spellcheck="false">
      <button class="eye-btn" onclick="togglePass()" tabindex="-1" type="button">👁</button>
    </div>
  </div>
  <div class="btn-row">
    <button class="secondary" onclick="backToScan()">← رجوع</button>
    <button class="primary" id="connectBtn" onclick="doConnect()">اتصال</button>
  </div>
</div>

<!-- ═══ PHASE 3: Connecting ═══ -->
<div id="phase-connecting" class="card hidden">
  <div class="status-panel">
    <div class="progress-ring">
      <svg viewBox="0 0 72 72" width="72" height="72">
        <circle class="ring-bg" cx="36" cy="36" r="32"/>
        <circle class="ring-fill" cx="36" cy="36" r="32" id="ring"/>
      </svg>
      <div class="ring-label" id="ring-lbl">15</div>
    </div>
    <div class="status-title">جارٍ الاتصال…</div>
    <div class="status-sub" id="conn-ssid-lbl">—</div>
    <div class="tag yellow">⏳ يُرجى الانتظار</div>
  </div>
</div>

<!-- ═══ PHASE 4: Result ═══ -->
<div id="phase-result" class="card hidden">
  <div class="status-panel" id="result-panel"></div>
</div>

<script>
// ── State ──
let selectedSSID = '', selectedOpen = false, selectedChannel = 1;
let pollTimer = null, countTimer = null;
const TIMEOUT_SEC = 30;

function setStep(n) {
  [1,2,3].forEach(i => {
    const el = document.getElementById('s'+i);
    el.className = 'step' + (i < n ? ' done' : i === n ? ' active' : '');
  });
}
function showPhase(name) {
  ['scan','pass','connecting','result'].forEach(p =>
    document.getElementById('phase-'+p).classList.toggle('hidden', p !== name)
  );
}
function rssiBars(rssi) {
  const lvl = rssi >= -50 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : rssi >= -85 ? 1 : 0;
  const heights = [4, 7, 10, 14];
  return `<div class="rssi-bar">${heights.map((h,i) =>
    `<span style="height:${h}px" class="${i < lvl ? 'on' : ''}"></span>`).join('')}</div>`;
}
function escHtml(s) {
  const d = document.createElement('div'); d.textContent = s; return d.innerHTML;
}

// ── Scan ──
async function startScan() {
  document.getElementById('scan-loader').classList.remove('hidden');
  document.getElementById('net-list').innerHTML = '';
  document.getElementById('rescanBtn').disabled = true;
  try {
    const r = await fetch('/scan');
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const nets = await r.json();
    renderNetworks(nets);
  } catch(e) {
    document.getElementById('net-list').innerHTML =
      `<div style="color:var(--red);font-size:.85rem">فشل البحث: ${e.message}</div>`;
  } finally {
    document.getElementById('scan-loader').classList.add('hidden');
    document.getElementById('rescanBtn').disabled = false;
  }
}
function renderNetworks(nets) {
  const list = document.getElementById('net-list');
  if (!nets || nets.length === 0) {
    list.innerHTML = '<div style="color:var(--muted);font-size:.85rem">لم يتم العثور على شبكات</div>';
    return;
  }
  list.innerHTML = nets.map(n => `
    <div class="net-item" onclick="selectNet('${escHtml(n.ssid)}', ${n.open}, ${n.ch})" data-ssid="${escHtml(n.ssid)}">
      <div class="net-icon">${n.open ? '📶' : '📡'}</div>
      <div class="net-info">
        <div class="net-ssid">${escHtml(n.ssid)}</div>
        <div class="net-meta">Ch ${n.ch || '?'} · ${n.rssi} dBm</div>
      </div>
      ${rssiBars(n.rssi)}
      <div class="net-lock">${n.open ? '' : '🔒'}</div>
    </div>`).join('');
}

// ── Selection ──
function selectNet(ssid, isOpen, channel) {
  selectedSSID = ssid; selectedOpen = isOpen; selectedChannel = channel || 1;
  document.querySelectorAll('.net-item').forEach(el =>
    el.classList.toggle('selected', el.dataset.ssid === ssid)
  );
  document.getElementById('chosen-ssid-lbl').textContent = ssid;
  document.getElementById('inp-pass').value = '';
  document.getElementById('pass-field').classList.toggle('hidden', isOpen);
  setStep(2); showPhase('pass');
  if (!isOpen) setTimeout(() => document.getElementById('inp-pass').focus(), 100);
}
function backToScan() { setStep(1); showPhase('scan'); }
function togglePass() {
  const inp = document.getElementById('inp-pass');
  inp.type = inp.type === 'password' ? 'text' : 'password';
}

// ── Connect ──
async function doConnect() {
  const pass = document.getElementById('inp-pass').value;
  if (!selectedOpen && pass.length < 8) {
    document.getElementById('inp-pass').classList.add('error');
    return;
  }
  document.getElementById('inp-pass').classList.remove('error');
  document.getElementById('connectBtn').disabled = true;
  document.getElementById('conn-ssid-lbl').textContent = selectedSSID;
  showPhase('connecting');
  startCountdown(TIMEOUT_SEC);
  try {
    const body = new URLSearchParams({ ssid: selectedSSID, pass, ch: selectedChannel });
    const r = await fetch('/connect', { method: 'POST', body });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    startPoll();
  } catch(e) {
    clearTimers();
    showResult(false, '', 'فشل إرسال الطلب: ' + e.message);
  }
}

// ── Countdown ──
function startCountdown(sec) {
  let remaining = sec;
  const circumference = 2 * Math.PI * 32;
  const ring = document.getElementById('ring');
  const lbl  = document.getElementById('ring-lbl');
  const tick = () => {
    const offset = circumference * (1 - remaining / sec);
    ring.style.strokeDashoffset = circumference - offset;
    lbl.textContent = remaining;
    if (remaining > 0) { remaining--; countTimer = setTimeout(tick, 1000); }
  };
  tick();
}

// ── Poll ──
function startPoll() {
  let attempts = 0;
  const maxAttempts = TIMEOUT_SEC + 5;
  const poll = async () => {
    attempts++;
    try {
      const r = await fetch('/wifi-status');
      if (!r.ok) throw new Error();
      const d = await r.json();
      if (d.state === 'connected') {
        clearTimers(); setStep(3);
        showResult(true, d.ip, d.ssid); return;
      }
      if (d.state === 'failed' || attempts >= maxAttempts) {
        clearTimers(); showResult(false, '', selectedSSID); return;
      }
    } catch(_) {}
    pollTimer = setTimeout(poll, 1500);
  };
  pollTimer = setTimeout(poll, 2000);
}
function clearTimers() { clearTimeout(pollTimer); clearTimeout(countTimer); }

// ── Result ──
function showResult(success, ip, extra) {
  showPhase('result');
  const panel = document.getElementById('result-panel');
  if (success) {
    setStep(3);
    panel.innerHTML = `
      <div class="big-icon">✅</div>
      <div class="status-title">تم الاتصال بنجاح!</div>
      <div class="status-sub">متصل بـ <strong>${escHtml(extra)}</strong></div>
      <div class="ip-box">${ip}</div>
      <div class="tag green">🟢 متصل</div>
      <div class="status-sub" style="margin-top:8px">
        افتح المتصفح على<br><strong>http://${ip}</strong>
      </div>
      <button class="primary" style="margin-top:8px;max-width:220px"
        onclick="window.location='http://${ip}'">فتح لوحة التحكم →</button>`;
  } else {
    setStep(2);
    panel.innerHTML = `
      <div class="big-icon">❌</div>
      <div class="status-title">فشل الاتصال</div>
      <div class="status-sub">
        تعذّر الاتصال بـ <strong>${escHtml(extra)}</strong><br>
        تأكد من كلمة المرور وقرب الجهاز من الراوتر
      </div>
      <div class="tag red">🔴 فشل</div>
      <div class="btn-row" style="margin-top:12px">
        <button class="secondary" onclick="backToScan()">← المحاولة مجدداً</button>
        <button class="primary" onclick="retryPass()">إعادة كلمة المرور</button>
      </div>
      <div style="text-align:center;margin-top:14px;padding-top:12px;border-top:1px solid var(--border)">
        <button class="primary" onclick="skipToAP()" style="background:linear-gradient(135deg,var(--muted),#475569)">
          ⚡ استخدام الجهاز في وضع AP
        </button>
        <div style="font-size:.72rem;color:var(--muted);margin-top:6px">تجاوز واستخدام لوحة التحكم عبر شبكة الجهاز</div>
      </div>`;
  }
}
function retryPass() { showPhase('pass'); setStep(2);
  document.getElementById('inp-pass').value = '';
  document.getElementById('connectBtn').disabled = false;
  setTimeout(() => document.getElementById('inp-pass').focus(), 100);
}

// ── Skip to AP mode ──
async function skipToAP() {
  try {
    const r = await fetch('/use-ap', { method: 'POST' });
    if (!r.ok) throw new Error();
    window.location.href = '/';
  } catch(e) {
    alert('فشل: ' + e.message);
  }
}

// Boot
startScan();
</script>
</body>
</html>
)rawliteral";


// ═══════════════════════════════════════════════════════════════════
//  PAGE 2: Control Panel (Connected Mode)
// ═══════════════════════════════════════════════════════════════════
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ar" dir="rtl">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Induction Control Panel</title>
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

  header { text-align: center; margin-bottom: 24px; }
  .logo {
    font-size: 1.9rem; font-weight: 800;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .sub { color: var(--muted); font-size: .82rem; margin-top: 4px; }

  /* Status Bar */
  .status-bar {
    display: flex; align-items: center; gap: 8px; flex-wrap: wrap;
    background: var(--surface); border: 1px solid var(--border);
    border-radius: 999px; padding: 6px 16px; margin-bottom: 20px;
    font-size: .78rem; max-width: 460px; width: 100%;
  }
  .dot { width: 8px; height: 8px; border-radius: 50%; }
  .dot.green { background: var(--green); animation: pulse 2s infinite; }
  .dot.yellow{ background: var(--yellow); animation: pulse 1.2s infinite; }
  .dot.red   { background: var(--red); }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:.3} }
  .sep { color: var(--border); }

  /* Cards */
  .card { width: 100%; max-width: 460px; background: var(--surface);
          border: 1px solid var(--border); border-radius: var(--r);
          padding: 22px; margin-bottom: 14px; }
  .card-title { font-size: .72rem; font-weight: 700; text-transform: uppercase;
                letter-spacing: 1.2px; color: var(--accent); margin-bottom: 18px; }

  /* Live Grid */
  .live-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
  .live-item { background: var(--bg); border: 1px solid var(--border);
               border-radius: 8px; padding: 12px 14px; }
  .lv-label { font-size: .68rem; color: var(--muted); margin-bottom: 4px; }
  .lv-val { font-size: 1.1rem; font-weight: 700; font-variant-numeric: tabular-nums; }

  /* Run State Banner */
  .run-banner {
    width: 100%; max-width: 460px; border-radius: var(--r); padding: 14px 18px;
    margin-bottom: 14px; display: flex; align-items: center; justify-content: space-between;
    font-size: .9rem; font-weight: 700; transition: all .3s;
  }
  .run-banner.idle  { background: rgba(100,116,139,.12); color: var(--muted); border: 1px solid var(--border); }
  .run-banner.run   { background: rgba(34,197,94,.12); color: var(--green); border: 1px solid rgba(34,197,94,.3); }
  .run-banner.start { background: rgba(234,179,8,.12); color: var(--yellow); border: 1px solid rgba(234,179,8,.3); }

  /* Control Buttons */
  .ctrl-row { display: flex; gap: 10px; width: 100%; max-width: 460px; margin-bottom: 14px; }
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
  .btn-start:hover { box-shadow: 0 6px 20px rgba(34,197,94,.35); }
  .btn-stop  { background: linear-gradient(135deg, var(--red), #dc2626); color: #fff;
               box-shadow: 0 4px 16px rgba(239,68,68,.25); }
  .btn-stop:hover  { box-shadow: 0 6px 20px rgba(239,68,68,.35); }

  /* Fields */
  .field { margin-bottom: 18px; }
  .field:last-child { margin-bottom: 0; }
  .fh { display: flex; justify-content: space-between; align-items: baseline; margin-bottom: 7px; }
  label { font-size: .85rem; color: var(--muted); font-weight: 500; }
  .badge { font-size: .95rem; font-weight: 700; }
  .badge .u { font-size: .68rem; color: var(--muted); margin-right: 2px; }

  input[type="number"] {
    width: 100%; background: var(--bg); border: 1.5px solid var(--border);
    border-radius: 8px; color: var(--text); font-size: .95rem;
    padding: 10px 14px; outline: none; transition: border-color .2s; direction: ltr;
  }
  input[type="number"]:focus { border-color: var(--accent); }
  input[type="number"].err   { border-color: var(--red) !important; }

  input[type="range"] {
    -webkit-appearance: none; width: 100%; height: 6px;
    background: var(--border); border-radius: 3px; outline: none;
  }
  input[type="range"]::-webkit-slider-thumb {
    -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%;
    background: var(--accent); cursor: pointer;
    box-shadow: 0 0 0 3px rgba(249,115,22,.25); transition: box-shadow .2s;
  }
  input[type="range"]:active::-webkit-slider-thumb {
    box-shadow: 0 0 0 6px rgba(249,115,22,.3);
  }
  .errmsg { font-size: .73rem; color: var(--red); margin-top: 5px; display: none; }

  /* Buttons */
  .btn-row { display: flex; gap: 10px; width: 100%; max-width: 460px; }
  button.primary {
    flex: 1; padding: 13px; border: none; border-radius: 9px;
    background: linear-gradient(135deg, var(--accent), var(--accent2));
    color: #fff; font-size: .92rem; font-weight: 700;
    cursor: pointer; transition: opacity .2s, transform .1s;
  }
  button.secondary {
    padding: 13px 16px; border-radius: 9px; background: var(--surface);
    border: 1px solid var(--border); color: var(--muted);
    font-size: .85rem; font-weight: 600; cursor: pointer;
  }
  button:disabled { opacity: .4; cursor: not-allowed; }
  button:active:not(:disabled) { transform: scale(.97); }

  /* Reset WiFi */
  .reset-btn {
    font-size: .78rem; color: var(--muted); background: none;
    border: 1px solid var(--border); border-radius: 6px;
    padding: 6px 12px; cursor: pointer; margin-top: 8px;
    transition: all .2s;
  }
  .reset-btn:hover { color: var(--red); border-color: var(--red); background: rgba(239,68,68,.05); }

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
  .toast.ok  { border-color: var(--green); color: var(--green); }
  .toast.bad { border-color: var(--red);   color: var(--red); }
  .toast.warn{ border-color: var(--yellow); color: var(--yellow); }

  /* Safety Warning */
  .safety-note {
    width: 100%; max-width: 460px; background: rgba(239,68,68,.06);
    border: 1px solid rgba(239,68,68,.2); border-radius: 8px;
    padding: 10px 14px; font-size: .75rem; color: var(--red);
    margin-bottom: 14px; text-align: center;
  }
</style>
</head>
<body>

<header>
  <div class="logo">⚡ Induction Control</div>
  <div class="sub">ESP32 MCPWM Half-Bridge Driver</div>
</header>

<!-- Status Bar -->
<div class="status-bar">
  <div class="dot green" id="status-dot"></div>
  <span id="netInfo" style="color:var(--muted)">جارٍ الاتصال…</span>
  <span class="sep">|</span>
  <span id="runState" style="font-weight:700">—</span>
</div>

<!-- Safety Warning -->
<div class="safety-note">
  ⚠️ تحذير: تأكد من توصيل المبرد (Heat Sink) وعدم تشغيل الجهاز بدون حمل!
</div>

<!-- Run State Banner -->
<div class="run-banner idle" id="runBanner">
  <span id="runBannerText">الجهاز جاهز — اضغط ▶ لتشغيل</span>
  <span id="runBannerIcon">⚪</span>
</div>

<!-- Start / Stop Controls -->
<div class="ctrl-row">
  <button class="ctrl-btn btn-start" id="btnStart" onclick="doStart()">▶ تشغيل</button>
  <button class="ctrl-btn btn-stop"  id="btnStop"  onclick="doStop()" disabled>■ إيقاف</button>
</div>

<!-- Live Values -->
<div class="card">
  <div class="card-title">القيم الحالية</div>
  <div class="live-grid">
    <div class="live-item">
      <div class="lv-label">التردد</div>
      <div class="lv-val"><span id="lv-f">—</span> <small style="color:var(--muted);font-size:.68rem">Hz</small></div>
    </div>
    <div class="live-item">
      <div class="lv-label">نسبة الإشغال</div>
      <div class="lv-val"><span id="lv-d">—</span> <small style="color:var(--muted);font-size:.68rem">%</small></div>
    </div>
    <div class="live-item">
      <div class="lv-label">Dead Time</div>
      <div class="lv-val"><span id="lv-dt">—</span> <small style="color:var(--muted);font-size:.68rem">ns</small></div>
    </div>
    <div class="live-item">
      <div class="lv-label">Soft Start</div>
      <div class="lv-val"><span id="lv-ss">—</span> <small style="color:var(--muted);font-size:.68rem">ms</small></div>
    </div>
  </div>
</div>

<!-- Settings -->
<div class="card">
  <div class="card-title">الإعدادات</div>

  <div class="field">
    <div class="fh"><label>التردد</label><span class="badge"><span id="d-f">100000</span><span class="u">Hz</span></span></div>
    <input id="i-f" type="number" value="100000" min="1000" max="400000" step="1000"
           oninput="sync('f')" onchange="valid('f')">
    <div class="errmsg" id="e-f">يجب أن يكون بين 1,000 و 400,000 Hz</div>
  </div>

  <div class="field">
    <div class="fh"><label>نسبة الإشغال (Duty)</label><span class="badge"><span id="d-d">50</span><span class="u">%</span></span></div>
    <input id="i-d" type="range" min="0" max="95" step="0.5" value="50" oninput="sync('d')">
  </div>

  <div class="field">
    <div class="fh"><label>Dead Time</label><span class="badge"><span id="d-dt">500</span><span class="u">ns</span></span></div>
    <input id="i-dt" type="number" value="500" min="0" max="5000" step="100"
           oninput="sync('dt')" onchange="valid('dt')">
    <div class="errmsg" id="e-dt">يجب أن يكون بين 0 و 5,000 ns</div>
  </div>

  <div class="field">
    <div class="fh"><label>Soft Start</label><span class="badge"><span id="d-ss">3000</span><span class="u">ms</span></span></div>
    <input id="i-ss" type="number" value="3000" min="500" max="10000" step="100"
           oninput="sync('ss')" onchange="valid('ss')">
    <div class="errmsg" id="e-ss">يجب أن يكون بين 500 و 10,000 ms</div>
  </div>
</div>

<div class="btn-row">
  <button class="primary" id="applyBtn" onclick="applyAll()">💾 حفظ وتطبيق</button>
  <button class="secondary" onclick="fetchStatus()">🔄 تحديث</button>
</div>

<div style="text-align:center;margin-top:12px">
  <button class="reset-btn" onclick="resetWiFi()">🔧 إعادة إعداد الشبكة</button>
</div>

<div class="toast" id="toast"></div>

<script>
const LIMITS = { f:[1000,400000], dt:[0,5000], ss:[500,10000] };

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
function validAll() { return ['f','dt','ss'].every(k => valid(k)); }

let tTimer;
function toast(msg, type='ok') {
  const t = document.getElementById('toast');
  t.textContent = msg; t.className = 'toast show ' + type;
  clearTimeout(tTimer);
  tTimer = setTimeout(() => t.className = 'toast', 2800);
}

// ── Apply Settings ──
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
    if (!r.ok) throw new Error('HTTP ' + r.status);
    toast('✓ تم الحفظ بنجاح');
    setTimeout(fetchStatus, 600);
  } catch(e) { toast('خطأ: ' + e.message, 'bad'); }
  finally { btn.disabled = false; btn.textContent = '💾 حفظ وتطبيق'; }
}

// ── Start / Stop ──
async function doStart() {
  const btn = document.getElementById('btnStart');
  btn.disabled = true;
  try {
    const r = await fetch('/start');
    const d = await r.json();
    if (d.success) { toast('▶ تم التشغيل'); updateRunUI('starting'); }
    else { toast('الجهاز يعمل بالفعل', 'warn'); }
  } catch(e) { toast('فشل التشغيل', 'bad'); }
  finally { setTimeout(fetchStatus, 500); }
}
async function doStop() {
  const btn = document.getElementById('btnStop');
  btn.disabled = true;
  try {
    await fetch('/stop');
    toast('■ تم الإيقاف');
    updateRunUI('idle');
  } catch(e) { toast('فشل الإيقاف', 'bad'); }
  finally { setTimeout(fetchStatus, 500); }
}

// ── Update Run UI ──
function updateRunUI(state) {
  const banner     = document.getElementById('runBanner');
  const bannerText = document.getElementById('runBannerText');
  const bannerIcon = document.getElementById('runBannerIcon');
  const btnStart   = document.getElementById('btnStart');
  const btnStop    = document.getElementById('btnStop');
  const dot        = document.getElementById('status-dot');

  banner.className = 'run-banner ' + state;
  if (state === 'idle') {
    bannerText.textContent = 'الجهاز جاهز — اضغط ▶ لتشغيل';
    bannerIcon.textContent = '⚪';
    btnStart.disabled = false;
    btnStop.disabled  = true;
    dot.className = 'dot green';
  } else if (state === 'starting') {
    bannerText.textContent = 'جارٍ التشغيل التدريجي…';
    bannerIcon.textContent = '🔄';
    btnStart.disabled = true;
    btnStop.disabled  = false;
    dot.className = 'dot yellow';
  } else if (state === 'run') {
    bannerText.textContent = 'الجهاز يعمل — اضغط ■ للإيقاف';
    bannerIcon.textContent = '🟢';
    btnStart.disabled = true;
    btnStop.disabled  = false;
    dot.className = 'dot green';
  }
}

// ── Fetch Status ──
async function fetchStatus() {
  try {
    const r = await fetch('/status');
    if (!r.ok) throw new Error();
    const d = await r.json();

    document.getElementById('lv-f').textContent  = Number(d.frequency).toLocaleString();
    document.getElementById('lv-d').textContent  = parseFloat(d.duty).toFixed(1);
    document.getElementById('lv-dt').textContent = parseFloat(d.deadTime).toFixed(0);
    document.getElementById('lv-ss').textContent = parseFloat(d.softStartMs).toFixed(0);

    // Run state
    let runStateText = '⚪ جاهز';
    if (d.softStarting) { runStateText = '🔄 Soft Start…'; updateRunUI('starting'); }
    else if (d.running) { runStateText = '🟢 يعمل';       updateRunUI('run'); }
    else { runStateText = '⚪ جاهز'; updateRunUI('idle'); }
    document.getElementById('runState').textContent = runStateText;

    // Network info
    document.getElementById('netInfo').textContent =
      `📶 ${d.ssid || '—'} · ${d.ip || '—'} · ${d.rssi || 0} dBm`;

    // Sync inputs
    document.getElementById('i-f').value  = d.frequency;
    document.getElementById('i-d').value  = d.duty;
    document.getElementById('i-dt').value = d.deadTime;
    document.getElementById('i-ss').value = d.softStartMs;
    ['f','d','dt','ss'].forEach(sync);
  } catch(e) {
    document.getElementById('netInfo').textContent = '🔴 انقطع الاتصال';
    document.getElementById('status-dot').className = 'dot red';
  }
}

// ── Reset WiFi ──
async function resetWiFi() {
  if (!confirm('سيتم مسح بيانات الشبكة وإعادة تشغيل الجهاز في وضع الإعداد.\n\nهل أنت متأكد؟')) return;
  try {
    await fetch('/reset-wifi', { method: 'POST' });
    alert('جارٍ إعادة التشغيل…\n\nاتصل بشبكة Wi-Fi:\n📡 Induction_Setup');
  } catch(_) {}
}

// Boot
fetchStatus();
setInterval(fetchStatus, 2500);
</script>
</body>
</html>
)rawliteral";
