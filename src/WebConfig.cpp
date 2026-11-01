#include "WebConfig.h"
#include "AppConfig.h"
#include "LiveData.h"
#include "HitClient.h"
#include "SunSpecServer.h"
#include "StatusDisplay.h"
#include "TasmotaClient.h"
#include "MqttHa.h"
#include "HitControl.h"
#include "DebugLog.h"
#include "OtaUpdate.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>

namespace {
AsyncWebServer server(80);

// Shared 2026 UI — once in flash, linked by all pages
const char UI_CSS[] PROGMEM = R"CSS(
:root{
  --bg:#0c0f0d;--bg2:#121816;--panel:rgba(22,28,24,.72);--line:rgba(180,210,190,.12);
  --txt:#eef3ef;--muted:#8b9a90;--acc:#6ef0a5;--acc2:#2dd4a0;--warn:#f0b429;--danger:#ff6b6b;--cyan:#8ee7ff;
  --r:14px;--ease:cubic-bezier(.22,1,.36,1);
  --font:"Segoe UI Variable Text","Segoe UI","SF Pro Text",system-ui,sans-serif;
  --mono:ui-monospace,"Cascadia Mono","SF Mono",Consolas,monospace;
}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;min-height:100vh;font:15px/1.45 var(--font);color:var(--txt);background:
  radial-gradient(900px 520px at 12% -10%,rgba(45,212,160,.16),transparent 55%),
  radial-gradient(700px 480px at 100% 0%,rgba(110,180,255,.08),transparent 50%),
  linear-gradient(180deg,#101512 0%,var(--bg) 45%,#0a0c0b 100%);
  letter-spacing:-.01em}
body::before{content:"";position:fixed;inset:0;pointer-events:none;opacity:.35;background-image:url("data:image/svg+xml,%3Csvg viewBox='0 0 200 200' xmlns='http://www.w3.org/2000/svg'%3E%3Cfilter id='n'%3E%3CfeTurbulence type='fractalNoise' baseFrequency='.9' numOctaves='3' stitchTiles='stitch'/%3E%3C/filter%3E%3Crect width='100%25' height='100%25' filter='url(%23n)' opacity='.35'/%3E%3C/svg%3E");mix-blend-mode:overlay}
.wrap{position:relative;max-width:980px;margin:0 auto;padding:28px 20px 48px}
.top{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;align-items:flex-end;margin-bottom:22px}
.brand{font-size:.72rem;font-weight:600;letter-spacing:.14em;text-transform:uppercase;color:var(--acc2);margin:0 0 6px}
h1{margin:0;font-size:clamp(1.35rem,3vw,1.85rem);font-weight:650;letter-spacing:-.03em;line-height:1.15}
.sub,.muted,.meta,.hint{color:var(--muted)}.sub{margin:6px 0 0;font-size:.92rem}
nav{display:flex;gap:4px;flex-wrap:wrap;padding:4px;background:rgba(0,0,0,.25);border:1px solid var(--line);border-radius:12px;backdrop-filter:blur(10px)}
nav a{color:var(--muted);text-decoration:none;padding:8px 12px;border-radius:9px;font-size:.86rem;font-weight:550;transition:color .2s var(--ease),background .2s var(--ease)}
nav a:hover,nav a.on{color:var(--txt);background:rgba(110,240,165,.1)}
a{color:var(--cyan)}
.panel,.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r);padding:16px 18px;
  backdrop-filter:blur(14px) saturate(1.2);box-shadow:0 1px 0 rgba(255,255,255,.04) inset,0 12px 40px rgba(0,0,0,.22);
  transition:border-color .25s var(--ease),transform .25s var(--ease)}
.metrics{display:grid;gap:12px;grid-template-columns:repeat(auto-fit,minmax(148px,1fr));margin:16px 0}
.metric .k,.k{font-size:.68rem;font-weight:600;letter-spacing:.08em;text-transform:uppercase;color:var(--muted)}
.metric .v,.v{font:650 1.65rem/1.1 var(--mono);font-variant-numeric:tabular-nums;margin-top:8px;letter-spacing:-.02em}
.u{font:600 .8rem/1 var(--font);color:var(--muted);margin-left:2px}
.ip{font:650 clamp(1.4rem,4vw,1.9rem)/1.1 var(--mono);color:var(--cyan);letter-spacing:-.03em}
.badge{display:inline-block;margin-left:10px;padding:3px 9px;border-radius:8px;border:1px solid rgba(240,180,41,.35);
  background:rgba(240,180,41,.12);color:var(--warn);font:650 .68rem/1.2 var(--font);letter-spacing:.06em;vertical-align:middle}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin:14px 0 4px}
.chip{display:inline-flex;align-items:center;gap:7px;padding:6px 10px;border-radius:10px;border:1px solid var(--line);
  background:rgba(0,0,0,.28);color:var(--muted);font-size:.78rem;font-weight:550}
.dot{width:7px;height:7px;border-radius:50%;flex-shrink:0;box-shadow:0 0 0 3px rgba(0,0,0,.25)}
.dot.ok{background:var(--acc);box-shadow:0 0 10px rgba(110,240,165,.45)}.dot.bad{background:var(--danger)}
.flow{display:grid;grid-template-columns:1fr auto 1fr auto 1fr;gap:8px;align-items:stretch;margin:12px 0 16px}
.box{text-align:center;padding:14px 10px;border-radius:12px;border:1px solid var(--line);background:rgba(0,0,0,.28)}
.box span{display:block;font-size:.68rem;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);font-weight:600}
.box b{display:block;margin-top:8px;font:650 1.05rem/1 var(--mono);font-variant-numeric:tabular-nums}
.arr{color:rgba(110,240,165,.45);align-self:center;font-size:1.1rem}
.bar{height:6px;background:rgba(0,0,0,.4);border-radius:99px;overflow:hidden;margin-top:10px}
.fill{height:100%;width:0;border-radius:inherit;background:linear-gradient(90deg,var(--acc2),var(--acc));transition:width .5s var(--ease)}
.grid,.grid2{display:grid;gap:12px}.grid{grid-template-columns:repeat(auto-fit,minmax(260px,1fr))}
.grid2{grid-template-columns:repeat(auto-fit,minmax(210px,1fr));margin-top:12px}
.card strong{display:block;font-size:.95rem;font-weight:650;letter-spacing:-.01em;margin-bottom:2px}
label{display:block;font-size:.72rem;font-weight:600;letter-spacing:.04em;text-transform:uppercase;color:var(--muted);margin:14px 0 6px}
input,select{width:100%;padding:11px 12px;border-radius:10px;border:1px solid var(--line);background:rgba(0,0,0,.35);
  color:var(--txt);font:450 .95rem/1.3 var(--font);outline:none;transition:border-color .2s,box-shadow .2s}
input:focus,select:focus{border-color:rgba(110,240,165,.45);box-shadow:0 0 0 3px rgba(110,240,165,.12)}
input[type=checkbox]{width:auto;margin-right:8px;accent-color:var(--acc2)}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:18px;align-items:center}
button,.btn{appearance:none;border:0;cursor:pointer;padding:11px 16px;border-radius:10px;font:650 .9rem/1 var(--font);
  background:linear-gradient(180deg,#8af5b8,var(--acc2));color:#062416;box-shadow:0 1px 0 rgba(255,255,255,.25) inset;
  transition:transform .15s var(--ease),filter .15s}button:hover,.btn:hover{filter:brightness(1.05)}button:active{transform:scale(.98)}
button.sec,.btn.sec{background:rgba(255,255,255,.06);color:var(--txt);border:1px solid var(--line);box-shadow:none}
.hint{font-size:.78rem;margin:8px 0 0;line-height:1.45}
.ok{color:var(--acc)}.bad{color:var(--danger)}
code{font-family:var(--mono);font-size:.86em;padding:2px 6px;border-radius:6px;background:rgba(0,0,0,.35);border:1px solid var(--line)}
pre{color:var(--muted);white-space:pre-wrap;font:450 .78rem/1.4 var(--mono);max-height:220px;overflow:auto;margin:8px 0 0}
.steps{display:flex;gap:6px;margin:18px 0 8px}.steps i{flex:1;height:3px;border-radius:99px;background:rgba(255,255,255,.08);transition:background .3s}
.steps i.on{background:var(--acc)}
.hide{display:none}ul{margin:10px 0 0 18px;color:var(--muted);font-size:.9rem;line-height:1.5}
.big{font:650 clamp(1.5rem,3.5vw,2rem)/1.1 var(--mono);color:var(--acc);letter-spacing:-.03em}
table{width:100%;border-collapse:collapse;font-size:.82rem}th,td{padding:10px 8px;border-bottom:1px solid var(--line);text-align:left}
th{color:var(--muted);font-weight:600;font-size:.7rem;letter-spacing:.06em;text-transform:uppercase}
#probeBox{margin-top:12px;display:none}#bar{height:8px;background:rgba(0,0,0,.4);border-radius:99px;overflow:hidden;margin-top:14px}
#fill{height:100%;width:0;background:linear-gradient(90deg,var(--acc2),var(--acc));transition:width .15s}
@media(max-width:640px){.flow{grid-template-columns:1fr;}.arr{display:none;transform:rotate(90deg)}}
)CSS";

const char DASH_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Live · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
</head><body><div class="wrap">
<div class="top">
  <div>
    <p class="brand">HIT SunSpec Proxy</p>
    <h1>Live <span id="ver" class="muted" style="font-size:.7rem;font-weight:500"></span></h1>
    <p class="sub">HIT → Proxy :1502 → Consolinno · autark</p>
  </div>
  <nav><a class="on" href="/">Live</a><a href="/config">Config</a><a href="/debug">Debug</a><a href="/ota">OTA</a></nav>
</div>
<div class="ip" id="ip">…</div>
<div class="chips" id="dots"></div>
<div class="metrics" id="m"></div>
<div class="panel">
  <div class="k">Energiefluss</div>
  <div class="flow">
    <div class="box"><span>PV</span><b id="fPv">—</b></div><div class="arr">→</div>
    <div class="box"><span>Akku / Haus</span><b id="fBat">—</b></div><div class="arr">→</div>
    <div class="box"><span>Netz</span><b id="fGrid">—</b></div>
  </div>
  <div class="k">Batterie SoC</div>
  <div class="bar"><div class="fill" id="socBar"></div></div>
  <div class="meta" id="socTxt" style="margin-top:10px">—</div>
</div>
<div class="grid2">
  <div class="panel"><div class="k">Energie HIT</div><div id="en" class="meta" style="margin-top:10px">—</div></div>
  <div class="panel"><div class="k">Consolinno LPP</div><div id="lpp" class="meta" style="margin-top:10px">—</div></div>
  <div class="panel"><div class="k">Tasmota</div><div id="tas" class="meta" style="margin-top:10px">—</div></div>
  <div class="panel"><div class="k">Phasen</div><div id="ph" class="meta" style="margin-top:10px">—</div></div>
</div>
<div class="row">
  <button class="sec" type="button" onclick="poll()">Jetzt pollen</button>
  <span class="meta" id="age"></span>
</div>
</div>
<script>
const chip=(ok,l)=>`<span class="chip"><span class="dot ${ok?'ok':'bad'}"></span>${l}</span>`;
const w=n=>`${Number(n)||0} W`;
async function poll(){await fetch('/api/poll',{method:'POST'});refresh();}
async function refresh(){
  const s=await(await fetch('/api/status')).json();
  if(s.setupComplete===false){location.href='/setup';return;}
  document.getElementById('ver').textContent='v'+(s.version||'');
  document.getElementById('ip').innerHTML=(s.ipShown||'-')+(s.demoMode?' <span class="badge">DEMO</span>':'');
  document.getElementById('dots').innerHTML=
    chip(s.hitOk,'HIT')+chip(s.sunspecClient,'HEMS')+chip(s.mqttOk,'MQTT')+chip(s.tasmotaOk,'TAS')+chip(s.ethOk||s.wifiOk,'NET');
  document.getElementById('m').innerHTML=
    `<div class="panel metric"><div class="k">PV</div><div class="v">${s.pvW||0}<span class="u">W</span></div></div>
     <div class="panel metric"><div class="k">Batterie</div><div class="v">${s.batW||0}<span class="u">W</span></div></div>
     <div class="panel metric"><div class="k">Netz</div><div class="v">${s.gridW||0}<span class="u">W</span></div></div>
     <div class="panel metric"><div class="k">SoC</div><div class="v">${s.soc||0}<span class="u">%</span></div></div>`;
  document.getElementById('fPv').textContent=w(s.pvW);
  document.getElementById('fBat').textContent=w(s.batW);
  document.getElementById('fGrid').textContent=w(s.gridW);
  const soc=Math.max(0,Math.min(100,Number(s.soc)||0));
  document.getElementById('socBar').style.width=soc+'%';
  document.getElementById('socTxt').textContent=soc+' % · '+(s.status||'—');
  document.getElementById('en').innerHTML=`PV ${Number(s.energyPvKwh||0).toFixed(1)} kWh<br>Bezug ${Number(s.energyImpKwh||0).toFixed(1)} · Einsp. ${Number(s.energyExpKwh||0).toFixed(1)} kWh`;
  const lpp=(Number(s.wMaxLimPct||0)/100).toFixed(2);
  document.getElementById('lpp').innerHTML=`WMaxLimPct ${lpp}% · ${s.wMaxLimEna?'aktiv':'aus'}<br>HEMS ${s.sunspecClient?'verbunden':'wartet'}<br>${s.lastWriteNote||''}`;
  document.getElementById('tas').innerHTML=s.tasmotaOk
    ? `${s.tasmotaSensorKey||'Zähler'}: ${s.tasmotaPowerW} W<br>E_in ${Number(s.tasmotaTotalKwh).toFixed(3)} · E_out ${Number(s.tasmotaExportKwh).toFixed(3)} kWh`
    : 'aus / offline';
  document.getElementById('ph').innerHTML=`L1 ${Number(s.v1||0).toFixed(1)} · L2 ${Number(s.v2||0).toFixed(1)} · L3 ${Number(s.v3||0).toFixed(1)} V<br>${Number(s.i1||0).toFixed(1)} / ${Number(s.i2||0).toFixed(1)} / ${Number(s.i3||0).toFixed(1)} A`;
  document.getElementById('age').textContent='HIT vor '+Math.floor((s.lastHitAgeMs||0)/1000)+'s · Eth '+(s.ipEth||'-')+' · WiFi '+(s.ipWifi||'-');
}
refresh();setInterval(()=>{if(!document.hidden)refresh();},2000);
</script></body></html>
)HTML";

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Config · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
</head><body><div class="wrap">
<div class="top">
  <div>
    <p class="brand">HIT SunSpec Proxy</p>
    <h1>Config</h1>
    <p class="sub">HIT Pflicht · Tasmota / MQTT optional</p>
  </div>
  <nav><a href="/">Live</a><a class="on" href="/config">Config</a><a href="/debug">Debug</a><a href="/ota">OTA</a></nav>
</div>
<div id="live" class="panel" style="margin-bottom:14px">Lade…</div>
<form id="f" class="grid">
<div class="card"><strong>HIT Quelle</strong>
<label>Verbindung</label>
<select name="hitMode"><option value="0">TCP (Modbus TCP)</option><option value="1">Kabel RS485 (RTU)</option></select>
<label>HIT IP / Host</label><input name="hitHost" placeholder="192.0.2.50"/>
<label>HIT Port</label><input name="hitPort" type="number"/>
<label>Unit-ID</label><input name="hitUnitId" type="number"/>
<label>Poll (ms)</label><input name="hitPollMs" type="number"/>
<label>RS485 Baud</label><input name="hitRtuBaud" type="number"/>
<label><input name="demoMode" type="checkbox"/> <b>Demo-Modus</b> (Fake-Daten, kein HIT-Schreiben)</label>
<p class="hint">Für Board-/Web-/Display-Test ohne Hoymiles. SunSpec :1502 lokal ansprechbar; Consolinno nicht nötig.</p>
</div>
<div class="card"><strong>SunSpec / Consolinno</strong>
<label>Listen-Port (Proxy)</label><input name="sunspecPort" type="number"/>
<label>Unit-ID</label><input name="sunspecUnitId" type="number"/>
<label><input name="invertPowerSign" type="checkbox"/> Leistung invertieren</label>
<label>Gerätename</label><input name="deviceName"/>
</div>
<div class="card"><strong>Netz & Display</strong>
<label><input name="useEthernet" type="checkbox"/> Ethernet</label>
<label><input name="wifiEnabled" type="checkbox"/> WiFi</label>
<label><input name="wifiApMode" type="checkbox"/> AP-Modus</label>
<label><input name="setupComplete" type="checkbox"/> Setup abgeschlossen (QR-Screen aus)</label>
<label>WiFi SSID</label><input name="wifiSsid"/>
<label>WiFi / Display-Code (random, auf Display)</label><input name="wifiPass" readonly/>
<label>Web-User</label><input name="webUser"/>
<label>Web-Passwort (selbst gewählt, gemerkt)</label><input name="webPass" type="password" placeholder="leer = nur Display-Code"/>
<p class="hint">Login: User + Web-Passwort <b>oder</b> Display-Code. Display-Code bleibt auf dem Display sichtbar.</p>
<label><input name="displayEnabled" type="checkbox"/> Display</label>
<label>Helligkeit</label><input name="displayBrightness" type="number"/>
</div>
<div class="card"><strong>HIT Steuerung (Schreiben)</strong>
<label><input name="hitWriteEnabled" type="checkbox"/> Schreiben an HIT erlauben</label>
<label><input name="sunspecWriteToHit" type="checkbox"/> Consolinno Model-123 → HIT weiterleiten</label>
<label>Max Laden % (Reg 306)</label><input name="batMaxChargePct" type="number" min="10" max="100"/>
<label>Max Entladen % (Reg 307)</label><input name="batMaxDischargePct" type="number" min="10" max="100"/>
<label>Max SoC % (308)</label><input name="batMaxSoc" type="number" min="10" max="100"/>
<label>Min SoC % (309)</label><input name="batMinSoc" type="number" min="10" max="90"/>
<label><input name="apcEnabled" type="checkbox"/> APC Einspeiselimit (Reg 279)</label>
<label>APC Limit %×10 (280, 1000=100%)</label><input name="apcPwrLmtPct10" type="number" min="0" max="1000"/>
<p class="hint">Default AUS. APC liegt im Safety-Bereich 101–299 — nur bewusst aktivieren.</p>
<button class="sec" type="button" onclick="applyCtrl()">Jetzt an HIT schreiben</button>
</div>
<div class="card"><strong>Tasmota Zähler</strong>
<label><input name="tasmotaEnabled" type="checkbox"/> aktiv (Display)</label>
<label><input name="tasmotaMeterSpoof" type="checkbox"/> Meter-Spoof für Consolinno (optional)</label>
<label>Tasmota IP</label><input name="tasmotaHost" placeholder="192.0.2.60"/>
<label>Port</label><input name="tasmotaPort" type="number"/>
<label>Poll (ms)</label><input name="tasmotaPollMs" type="number"/>
<label>User (optional)</label><input name="tasmotaUser"/>
<label>Passwort (optional)</label><input name="tasmotaPass" type="password"/>
<p class="hint">E320: Power / E_in / E_out. Meter-Spoof hängt SunSpec Model 201 an :1502.</p>
<button class="sec" type="button" onclick="probeTas()" style="margin-top:10px">One-Click: Tasmota auslesen</button>
<div id="probeBox"><div id="probeSum"></div><pre id="probeRaw"></pre></div>
</div>
<div class="card"><strong>MQTT → Home Assistant</strong>
<label><input name="mqttEnabled" type="checkbox"/> aktiv</label>
<label><input name="mqttDiscovery" type="checkbox"/> HA Discovery</label>
<label>Broker IP</label><input name="mqttHost" placeholder="192.0.2.10"/>
<label>Port</label><input name="mqttPort" type="number"/>
<label>User</label><input name="mqttUser"/>
<label>Passwort</label><input name="mqttPass" type="password"/>
<label>Topic-Basis</label><input name="mqttBase" placeholder="hitproxy"/>
<label>Publish (ms)</label><input name="mqttPublishMs" type="number"/>
<p class="hint">State: &lt;basis&gt;/state · Discovery unter homeassistant/sensor/…</p>
</div>
</form>
<div class="row">
<button type="button" onclick="save()">Speichern</button>
<button class="sec" type="button" onclick="poll()">Jetzt pollen</button>
<button class="sec" type="button" onclick="load()">Neu laden</button>
</div>
<p class="sub">Consolinno: Proxy-IP vom Display, Port 1502, Unit 1. HIT Pflicht · Tasmota/MQTT optional.</p>
</div>
<script>
async function load(){const c=await(await fetch('/api/config')).json();const f=document.getElementById('f');
for(const[k,v] of Object.entries(c)){const el=f.elements.namedItem(k);if(!el)continue;if(el.type==='checkbox')el.checked=!!v;else el.value=v;}refresh();}
async function save(){const f=document.getElementById('f');const body={};
[...f.elements].forEach(el=>{if(!el.name)return;if(el.type==='checkbox')body[el.name]=el.checked;else if(el.type==='number')body[el.name]=Number(el.value);else body[el.name]=el.value;});
await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});alert('Gespeichert');refresh();}
async function poll(){await fetch('/api/poll',{method:'POST'});refresh();}
async function applyCtrl(){
  const f=document.getElementById('f');
  await save();
  const r=await(await fetch('/api/control/apply',{method:'POST'})).json();
  alert(r.ok?'An HIT geschrieben':('Fehler: '+(r.error||'?')));
}
async function probeTas(){
  const f=document.getElementById('f');
  const body={tasmotaHost:f.tasmotaHost.value,tasmotaPort:Number(f.tasmotaPort.value),
    tasmotaUser:f.tasmotaUser.value,tasmotaPass:f.tasmotaPass.value};
  const r=await(await fetch('/api/tasmota/probe',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).json();
  document.getElementById('probeBox').style.display='block';
  document.getElementById('probeSum').textContent=r.summary|| (r.ok?'OK':'Fehler');
  document.getElementById('probeSum').style.color=r.ok?'#3dde7d':'#ff5a5a';
  document.getElementById('probeRaw').textContent=r.raw||'';
  if(r.ok){f.tasmotaEnabled.checked=true;refresh();}
}
async function refresh(){const s=await(await fetch('/api/status')).json();
const chip=(ok,l)=>`<span class="chip"><span class="dot ${ok?'ok':'bad'}"></span>${l}</span>`;
document.getElementById('live').innerHTML=`<div class="ip">${s.ipShown||'-'}${s.demoMode?' <span class="badge">DEMO</span>':''}</div>
<div class="chips">${chip(s.hitOk,'HIT')}${chip(s.sunspecClient,'HEMS')}${chip(s.mqttOk,'MQTT')}${chip(s.tasmotaOk,'TAS')}${chip(s.ethOk||s.wifiOk,'NET')}</div>
<div class="meta" style="margin-top:8px"><b>${s.pvW} W</b> PV · Bat ${s.batW} W · Netz ${s.gridW} W · SoC ${s.soc}% · ${Number(s.energyPvKwh).toFixed(1)} kWh</div>
<pre>HIT Bezug ${Number(s.energyImpKwh||0).toFixed(1)} / Einsp. ${Number(s.energyExpKwh||0).toFixed(1)} kWh
Tasmota ${s.tasmotaSensorKey||''}: ${s.tasmotaOk? (s.tasmotaPowerW+' W · E_in '+Number(s.tasmotaTotalKwh).toFixed(3)+' · E_out '+Number(s.tasmotaExportKwh).toFixed(3)+' kWh') : 'aus'}
HIT ${s.hitHost||''} · ${s.status||''}</pre>`;}
async function boot(){const s=await(await fetch('/api/status')).json();if(s.setupComplete===false)location.href='/setup';else load();}
boot();setInterval(()=>{if(document.hidden)return;refresh();},5000);
</script></body></html>
)HTML";

const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Einrichtung · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>.wrap{max-width:560px}.card{margin-top:14px}</style>
</head><body><div class="wrap">
<p class="brand">HIT SunSpec Proxy</p>
<h1>Einrichtung</h1>
<p class="sub">Hoymiles HIT → dieser Proxy → Consolinno — ohne HA / Node-RED.</p>
<div class="steps" id="bar"><i></i><i></i><i></i><i></i><i></i></div>

<div class="card" id="s0">
<strong>1 · Willkommen</strong>
<p class="hint">Nach der Einrichtung pollt der Proxy den HIT und stellt SunSpec auf Port <code>1502</code> bereit. Consolinno spricht nur noch diesen Proxy an.</p>
<ul>
<li>Display zeigt die Proxy-IP (Ethernet bevorzugt)</li>
<li>HA/MQTT optional — nicht nötig für Consolinno</li>
</ul>
<div class="row"><button type="button" onclick="go(1)">Weiter</button></div>
</div>

<div class="card hide" id="s1">
<strong>2 · Zugang</strong>
<p class="hint">Display-Code (Login-Fallback, auf dem Display):</p>
<div class="ip" id="dcode">…</div>
<label>Web-User</label><input id="webUser" value="admin"/>
<label>Eigenes Web-Passwort (gemerkt)</label><input id="webPass" type="password" placeholder="empfohlen"/>
<p class="hint">Login danach: User + Passwort <b>oder</b> Display-Code.</p>
<div class="row"><button class="sec" type="button" onclick="go(0)">Zurück</button><button type="button" onclick="go(2)">Weiter</button></div>
</div>

<div class="card hide" id="s2">
<strong>3 · Hoymiles HIT</strong>
<label><input id="demoMode" type="checkbox"/> Erst nur Demo (Fake-Daten, kein HIT)</label>
<label>HIT IP</label><input id="hitHost" placeholder="z.B. 192.168.x.x"/>
<label>Port</label><input id="hitPort" type="number" value="502"/>
<label>Unit-ID</label><input id="hitUnitId" type="number" value="1"/>
<div class="row">
<button class="sec" type="button" onclick="go(1)">Zurück</button>
<button class="sec" type="button" onclick="testHit()">HIT testen</button>
<button type="button" onclick="go(3)">Weiter</button>
</div>
<p class="hint" id="hitMsg"></p>
</div>

<div class="card hide" id="s3">
<strong>4 · Consolinno anbinden</strong>
<p class="hint">In der Consolinno-App Gerät als Fronius / SunSpec Hybrid (o.ä.) mit:</p>
<div class="ip" id="proxyIp">…</div>
<ul>
<li>IP = diese Proxy-IP (Display)</li>
<li>Port = <code id="ssPort">1502</code></li>
<li>Unit / Slave = <code>1</code></li>
</ul>
<p class="hint">Alten Node-RED-SunSpec-Pfad danach abschalten. HIT nicht direkt in Consolinno eintragen.</p>
<div class="row"><button class="sec" type="button" onclick="go(2)">Zurück</button><button type="button" onclick="go(4)">Weiter</button></div>
</div>

<div class="card hide" id="s4">
<strong>5 · Fertig — autark</strong>
<p class="hint">Speichern schaltet den Setup-Screen am Display aus. Der Proxy läuft dann dauerhaft HIT ↔ Consolinno.</p>
<ul>
<li>Optional später: MQTT, Tasmota, OTA unter der normalen Config</li>
<li>Assistent erneut: Config → „Setup abgeschlossen“ aus</li>
</ul>
<div class="row">
<button class="sec" type="button" onclick="go(3)">Zurück</button>
<button type="button" onclick="finish()">Einrichtung abschließen</button>
</div>
<p class="hint" id="finMsg"></p>
</div>
</div>
<script>
let step=0;
function paint(){
  for(let i=0;i<5;i++){document.getElementById('s'+i).classList.toggle('hide',i!==step);document.querySelectorAll('#bar i')[i].classList.toggle('on',i<=step);}
}
function go(n){step=n;paint();}
async function boot(){
  const [c,s]=await Promise.all([fetch('/api/config').then(r=>r.json()),fetch('/api/status').then(r=>r.json())]);
  if(c.setupComplete){location.href='/';return;}
  document.getElementById('dcode').textContent=c.displayCode||c.wifiPass||'—';
  document.getElementById('webUser').value=c.webUser||'admin';
  document.getElementById('hitHost').value=(c.hitHost&&!c.hitHost.startsWith('192.0.2'))?c.hitHost:'';
  document.getElementById('hitPort').value=c.hitPort||502;
  document.getElementById('hitUnitId').value=c.hitUnitId||1;
  document.getElementById('demoMode').checked=!!c.demoMode;
  document.getElementById('proxyIp').textContent=s.ipShown||'—';
  document.getElementById('ssPort').textContent=c.sunspecPort||1502;
  paint();
}
async function testHit(){
  const msg=document.getElementById('hitMsg');
  if(document.getElementById('demoMode').checked){msg.innerHTML='<span class="ok">Demo gewählt — HIT-Test übersprungen.</span>';return;}
  const body={hitHost:document.getElementById('hitHost').value.trim(),hitPort:Number(document.getElementById('hitPort').value),
    hitUnitId:Number(document.getElementById('hitUnitId').value),demoMode:false,hitMode:0};
  if(!body.hitHost){msg.innerHTML='<span class="bad">HIT-IP fehlt</span>';return;}
  msg.textContent='Speichern & pollen…';
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  await fetch('/api/poll',{method:'POST'});
  const s=await(await fetch('/api/status')).json();
  msg.innerHTML=s.hitOk?('<span class="ok">HIT ok — '+s.pvW+' W PV, SoC '+s.soc+' %</span>'):('<span class="bad">Kein HIT — IP/Netz prüfen ('+(s.status||'')+')</span>');
}
async function finish(){
  const fin=document.getElementById('finMsg'); fin.textContent='Speichere…';
  const body={
    webUser:document.getElementById('webUser').value.trim()||'admin',
    webPass:document.getElementById('webPass').value,
    hitHost:document.getElementById('hitHost').value.trim(),
    hitPort:Number(document.getElementById('hitPort').value)||502,
    hitUnitId:Number(document.getElementById('hitUnitId').value)||1,
    hitMode:0,
    demoMode:document.getElementById('demoMode').checked,
    sunspecPort:1502,sunspecUnitId:1,
    setupComplete:true,wifiApMode:false
  };
  if(!body.demoMode && !body.hitHost){fin.innerHTML='<span class="bad">HIT-IP oder Demo nötig</span>';go(2);return;}
  const r=await(await fetch('/api/setup/finish',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).json();
  if(!r.ok){fin.innerHTML='<span class="bad">'+(r.error||'Fehler')+'</span>';return;}
  fin.innerHTML='<span class="ok">Fertig — weiter zum Live-Dashboard…</span>';
  setTimeout(()=>location.href='/',1200);
}
boot();
</script></body></html>
)HTML";

const char DEBUG_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Write-Debug · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>.card{margin-bottom:12px}</style>
</head><body><div class="wrap">
<div class="top">
  <div>
    <p class="brand">HIT SunSpec Proxy</p>
    <h1>§14a Write-Debug</h1>
    <p class="sub">SunSpec Model 123 Writes von Consolinno auf :1502</p>
  </div>
  <nav><a href="/">Live</a><a href="/config">Config</a><a class="on" href="/debug">Debug</a><a href="/ota">OTA</a></nav>
</div>
<div class="card" id="sum">Lade…</div>
<div class="card">
<div class="row" style="margin-top:0;margin-bottom:12px">
<button class="sec" type="button" onclick="refresh()">Aktualisieren</button>
<button class="sec" type="button" onclick="clr()">Log leeren</button>
</div>
<table><thead><tr><th>Alter</th><th>FC</th><th>Addr</th><th>Wert</th><th>Hinweis</th></tr></thead>
<tbody id="tb"></tbody></table>
</div>
<script>
function age(ms){const s=Math.floor((Date.now()-window._boot-ms)/1000);if(s<0)return ms+'ms';if(s<60)return s+'s';return Math.floor(s/60)+'m';}
async function refresh(){
  const d=await(await fetch('/api/debug')).json();
  window._boot=Date.now()-d.uptimeMs;
  const pct=(d.wMaxLimPct/100).toFixed(2);
  const ena=d.wMaxLimEna;
  document.getElementById('sum').innerHTML=`
    <div class="big">${ena?'BEGRENZT':'Frei'} · ${pct} %</div>
    <div class="muted">WMaxLimPct_Ena=${ena?1:0} · raw=${d.wMaxLimPct} (SF-2) · HEMS-Client ${d.sunspecClient?'verbunden':'—'} · Writes ${d.count}
    ${d.lastNote?('<br>Zuletzt: '+d.lastNote+' (Addr '+d.lastAddr+' = '+d.lastVal+')'):'<br>Noch kein Write empfangen — in Consolinno §14a/LPP testen.'}</div>
    <div class="muted" style="margin-top:8px">HIT-Weiterleitung: ${d.sunspecWriteToHit?'an':'aus'} · Write-Enable: ${d.hitWriteEnabled?'an':'aus'}</div>`;
  const tb=document.getElementById('tb'); tb.innerHTML='';
  (d.events||[]).slice().reverse().forEach(e=>{
    const tr=document.createElement('tr');
    tr.innerHTML=`<td>${age(e.t)}</td><td>${e.fc}</td><td>${e.addr}</td><td>${e.val}</td><td>${e.note||''}</td>`;
    tb.appendChild(tr);
  });
}
async function clr(){await fetch('/api/debug/clear',{method:'POST'});refresh();}
refresh();setInterval(refresh,2000);
</script></body></html>
)HTML";

bool passOk(const AppConfig &c, const String &pass) {
  if (c.displayCode[0] && pass == c.displayCode) return true;
  if (c.webPass[0] && pass == c.webPass) return true;
  if (c.wifiPass[0] && pass == c.wifiPass) return true;
  return false;
}

bool requireAuth(AsyncWebServerRequest *req) {
  auto &c = ConfigStore::get();
  if (!req->hasHeader("Authorization")) {
    req->requestAuthentication("HIT-Proxy");
    return false;
  }
  String h = req->getHeader("Authorization")->value();
  if (!h.startsWith("Basic ")) {
    req->requestAuthentication("HIT-Proxy");
    return false;
  }
  String b64 = h.substring(6);
  b64.trim();
  unsigned char decoded[128];
  size_t olen = 0;
  if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &olen,
                            (const unsigned char *)b64.c_str(), b64.length()) != 0) {
    req->requestAuthentication("HIT-Proxy");
    return false;
  }
  decoded[olen] = 0;
  String cred = String((char *)decoded);
  int colon = cred.indexOf(':');
  if (colon < 0) {
    req->requestAuthentication("HIT-Proxy");
    return false;
  }
  String user = cred.substring(0, colon);
  String pass = cred.substring(colon + 1);
  if (user != c.webUser || !passOk(c, pass)) {
    req->requestAuthentication("HIT-Proxy");
    return false;
  }
  return true;
}

void applyJson(JsonObjectConst o, AppConfig &c) {
  if (!o["useEthernet"].isNull()) c.useEthernet = o["useEthernet"];
  if (!o["wifiEnabled"].isNull()) c.wifiEnabled = o["wifiEnabled"];
  if (!o["wifiApMode"].isNull()) c.wifiApMode = o["wifiApMode"];
  if (!o["setupComplete"].isNull()) c.setupComplete = o["setupComplete"];
  if (o["wifiSsid"].is<const char *>()) strlcpy(c.wifiSsid, o["wifiSsid"], sizeof(c.wifiSsid));
  // wifiPass / displayCode: random — nicht aus Form überschreiben außer explizit
  if (o["webUser"].is<const char *>()) strlcpy(c.webUser, o["webUser"], sizeof(c.webUser));
  if (o["webPass"].is<const char *>()) strlcpy(c.webPass, o["webPass"], sizeof(c.webPass));
  if (!o["hitWriteEnabled"].isNull()) c.hitWriteEnabled = o["hitWriteEnabled"];
  if (!o["sunspecWriteToHit"].isNull()) c.sunspecWriteToHit = o["sunspecWriteToHit"];
  if (!o["batMaxChargePct"].isNull()) c.batMaxChargePct = o["batMaxChargePct"];
  if (!o["batMaxDischargePct"].isNull()) c.batMaxDischargePct = o["batMaxDischargePct"];
  if (!o["batMaxSoc"].isNull()) c.batMaxSoc = o["batMaxSoc"];
  if (!o["batMinSoc"].isNull()) c.batMinSoc = o["batMinSoc"];
  if (!o["apcEnabled"].isNull()) c.apcEnabled = o["apcEnabled"];
  if (!o["apcPwrLmtPct10"].isNull()) c.apcPwrLmtPct10 = o["apcPwrLmtPct10"];
  if (!o["hitMode"].isNull()) c.hitMode = static_cast<HitLinkMode>(o["hitMode"].as<int>());
  if (o["hitHost"].is<const char *>()) strlcpy(c.hitHost, o["hitHost"], sizeof(c.hitHost));
  if (!o["hitPort"].isNull()) c.hitPort = o["hitPort"];
  if (!o["hitUnitId"].isNull()) c.hitUnitId = o["hitUnitId"];
  if (!o["hitPollMs"].isNull()) c.hitPollMs = o["hitPollMs"];
  if (!o["hitRtuBaud"].isNull()) c.hitRtuBaud = o["hitRtuBaud"];
  if (!o["demoMode"].isNull()) c.demoMode = o["demoMode"];
  if (!o["sunspecPort"].isNull()) c.sunspecPort = o["sunspecPort"];
  if (!o["sunspecUnitId"].isNull()) c.sunspecUnitId = o["sunspecUnitId"];
  if (!o["invertPowerSign"].isNull()) c.invertPowerSign = o["invertPowerSign"];
  if (!o["displayEnabled"].isNull()) c.displayEnabled = o["displayEnabled"];
  if (!o["displayBrightness"].isNull()) c.displayBrightness = o["displayBrightness"];
  if (!o["tasmotaEnabled"].isNull()) c.tasmotaEnabled = o["tasmotaEnabled"];
  if (!o["tasmotaMeterSpoof"].isNull()) c.tasmotaMeterSpoof = o["tasmotaMeterSpoof"];
  if (o["tasmotaHost"].is<const char *>()) strlcpy(c.tasmotaHost, o["tasmotaHost"], sizeof(c.tasmotaHost));
  if (!o["tasmotaPort"].isNull()) c.tasmotaPort = o["tasmotaPort"];
  if (!o["tasmotaPollMs"].isNull()) c.tasmotaPollMs = o["tasmotaPollMs"];
  if (o["tasmotaUser"].is<const char *>()) strlcpy(c.tasmotaUser, o["tasmotaUser"], sizeof(c.tasmotaUser));
  if (o["tasmotaPass"].is<const char *>()) strlcpy(c.tasmotaPass, o["tasmotaPass"], sizeof(c.tasmotaPass));
  if (!o["mqttEnabled"].isNull()) c.mqttEnabled = o["mqttEnabled"];
  if (!o["mqttDiscovery"].isNull()) c.mqttDiscovery = o["mqttDiscovery"];
  if (o["mqttHost"].is<const char *>()) strlcpy(c.mqttHost, o["mqttHost"], sizeof(c.mqttHost));
  if (!o["mqttPort"].isNull()) c.mqttPort = o["mqttPort"];
  if (o["mqttUser"].is<const char *>()) strlcpy(c.mqttUser, o["mqttUser"], sizeof(c.mqttUser));
  if (o["mqttPass"].is<const char *>()) strlcpy(c.mqttPass, o["mqttPass"], sizeof(c.mqttPass));
  if (o["mqttBase"].is<const char *>()) strlcpy(c.mqttBase, o["mqttBase"], sizeof(c.mqttBase));
  if (!o["mqttPublishMs"].isNull()) c.mqttPublishMs = o["mqttPublishMs"];
  if (o["deviceName"].is<const char *>()) strlcpy(c.deviceName, o["deviceName"], sizeof(c.deviceName));
}
} // namespace

namespace WebConfig {

void begin() {
  // Public stylesheet (no auth) — pages stay readable right after login
  server.on("/a.css", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/css", UI_CSS);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    if (!ConfigStore::get().setupComplete) {
      req->redirect("/setup");
      return;
    }
    req->send_P(200, "text/html", DASH_HTML);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    if (!ConfigStore::get().setupComplete) {
      req->redirect("/setup");
      return;
    }
    req->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send_P(200, "text/html", SETUP_HTML);
  });

  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send_P(200, "text/html", DEBUG_HTML);
  });

  server.on("/api/debug", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    auto &c = ConfigStore::get();
    String ev = DebugLog::toJson(40);
    String out = "{";
    out += "\"uptimeMs\":"; out += millis();
    out += ",\"wMaxLimPct\":"; out += gLive.wMaxLimPct;
    out += ",\"wMaxLimEna\":"; out += gLive.wMaxLimEna ? "true" : "false";
    out += ",\"sunspecClient\":"; out += gLive.sunspecClient ? "true" : "false";
    out += ",\"hitWriteEnabled\":"; out += c.hitWriteEnabled ? "true" : "false";
    out += ",\"sunspecWriteToHit\":"; out += c.sunspecWriteToHit ? "true" : "false";
    out += ",\"count\":"; out += DebugLog::count();
    out += ",\"lastAddr\":"; out += DebugLog::lastAddr();
    out += ",\"lastVal\":"; out += DebugLog::lastValue();
    out += ",\"lastNote\":\""; out += DebugLog::lastNote(); out += "\"";
    out += ",\"events\":"; out += ev;
    out += "}";
    req->send(200, "application/json", out);
  });

  server.on("/api/debug/clear", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    DebugLog::clear();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/setup/finish", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, data, len)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
              }
              auto &c = ConfigStore::get();
              applyJson(doc.as<JsonObjectConst>(), c);
              c.setupComplete = true;
              c.sunspecPort = c.sunspecPort ? c.sunspecPort : 1502;
              c.sunspecUnitId = c.sunspecUnitId ? c.sunspecUnitId : 1;
              // Autark-Pfad: Assistent beendet AP-Zwang; LAN/Eth bleibt
              if (!doc["wifiApMode"].isNull()) c.wifiApMode = doc["wifiApMode"];
              else c.wifiApMode = false;
              ConfigStore::save();
              gLive.setupScreen = false;
              HitClient::applyConfig(c);
              HitControl::applyConfig(c);
              SunSpecServer::applyConfig(c);
              StatusDisplay::applyConfig(c);
              TasmotaClient::applyConfig(c);
              MqttHa::applyConfig(c);
              HitClient::pollNow();
              SunSpecServer::rebuildMap();
              req->send(200, "application/json", "{\"ok\":true}");
            });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    auto &c = ConfigStore::get();
    doc["ipShown"] = gLive.ipShown;
    doc["setupComplete"] = c.setupComplete;
    doc["setupScreen"] = gLive.setupScreen;
    doc["ipEth"] = gLive.ipEth;
    doc["ipWifi"] = gLive.ipWifi;
    doc["hitHost"] = c.hitHost;
    doc["hitOk"] = gLive.hitOk;
    doc["ethOk"] = gLive.ethOk;
    doc["wifiOk"] = gLive.wifiOk;
    doc["sunspecClient"] = gLive.sunspecClient;
    doc["mqttOk"] = gLive.mqttOk;
    doc["pvW"] = gLive.pvW;
    doc["batW"] = gLive.batW;
    doc["soc"] = gLive.soc;
    doc["gridW"] = gLive.gridW;
    doc["v1"] = gLive.v1;
    doc["v2"] = gLive.v2;
    doc["v3"] = gLive.v3;
    doc["i1"] = gLive.i1;
    doc["i2"] = gLive.i2;
    doc["i3"] = gLive.i3;
    doc["energyPvKwh"] = gLive.energyPvKwh;
    doc["energyImpKwh"] = gLive.energyImpKwh;
    doc["energyExpKwh"] = gLive.energyExpKwh;
    doc["tasmotaOk"] = gLive.tasmotaOk;
    doc["tasmotaPowerW"] = gLive.tasmotaPowerW;
    doc["tasmotaTotalKwh"] = gLive.tasmotaTotalKwh;
    doc["tasmotaExportKwh"] = gLive.tasmotaExportKwh;
    doc["tasmotaSensorKey"] = gLive.tasmotaSensorKey;
    doc["wMaxLimPct"] = gLive.wMaxLimPct;
    doc["wMaxLimEna"] = gLive.wMaxLimEna;
    doc["lastWriteNote"] = DebugLog::lastNote();
    doc["lastHitAgeMs"] = gLive.lastHitMs ? (millis() - gLive.lastHitMs) : 0;
    doc["status"] = gLive.status;
    doc["version"] = PROXY_VERSION;
    doc["otaBusy"] = OtaUpdate::inProgress();
    doc["demoMode"] = c.demoMode;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    auto &c = ConfigStore::get();
    JsonDocument doc;
    doc["useEthernet"] = c.useEthernet;
    doc["wifiEnabled"] = c.wifiEnabled;
    doc["wifiApMode"] = c.wifiApMode;
    doc["setupComplete"] = c.setupComplete;
    doc["wifiSsid"] = c.wifiSsid;
    doc["wifiPass"] = c.displayCode[0] ? c.displayCode : c.wifiPass; // UI zeigt Display-Code
    doc["webUser"] = c.webUser;
    doc["webPass"] = c.webPass;
    doc["displayCode"] = c.displayCode;
    doc["hitWriteEnabled"] = c.hitWriteEnabled;
    doc["sunspecWriteToHit"] = c.sunspecWriteToHit;
    doc["batMaxChargePct"] = c.batMaxChargePct;
    doc["batMaxDischargePct"] = c.batMaxDischargePct;
    doc["batMaxSoc"] = c.batMaxSoc;
    doc["batMinSoc"] = c.batMinSoc;
    doc["apcEnabled"] = c.apcEnabled;
    doc["apcPwrLmtPct10"] = c.apcPwrLmtPct10;
    doc["hitMode"] = int(c.hitMode);
    doc["hitHost"] = c.hitHost;
    doc["hitPort"] = c.hitPort;
    doc["hitUnitId"] = c.hitUnitId;
    doc["hitPollMs"] = c.hitPollMs;
    doc["hitRtuBaud"] = c.hitRtuBaud;
    doc["demoMode"] = c.demoMode;
    doc["sunspecPort"] = c.sunspecPort;
    doc["sunspecUnitId"] = c.sunspecUnitId;
    doc["invertPowerSign"] = c.invertPowerSign;
    doc["displayEnabled"] = c.displayEnabled;
    doc["displayBrightness"] = c.displayBrightness;
    doc["tasmotaEnabled"] = c.tasmotaEnabled;
    doc["tasmotaMeterSpoof"] = c.tasmotaMeterSpoof;
    doc["tasmotaHost"] = c.tasmotaHost;
    doc["tasmotaPort"] = c.tasmotaPort;
    doc["tasmotaPollMs"] = c.tasmotaPollMs;
    doc["tasmotaUser"] = c.tasmotaUser;
    doc["tasmotaPass"] = c.tasmotaPass;
    doc["mqttEnabled"] = c.mqttEnabled;
    doc["mqttDiscovery"] = c.mqttDiscovery;
    doc["mqttHost"] = c.mqttHost;
    doc["mqttPort"] = c.mqttPort;
    doc["mqttUser"] = c.mqttUser;
    doc["mqttPass"] = c.mqttPass;
    doc["mqttBase"] = c.mqttBase;
    doc["mqttPublishMs"] = c.mqttPublishMs;
    doc["deviceName"] = c.deviceName;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, data, len)) {
                req->send(400, "text/plain", "bad json");
                return;
              }
              auto &c = ConfigStore::get();
              applyJson(doc.as<JsonObjectConst>(), c);
              // Speichern = Setup erledigt (QR-Screen aus), außer explizit false
              if (doc["setupComplete"].isNull()) c.setupComplete = true;
              ConfigStore::save();
              gLive.setupScreen = !c.setupComplete;
              HitClient::applyConfig(c);
              HitControl::applyConfig(c);
              SunSpecServer::applyConfig(c);
              StatusDisplay::applyConfig(c);
              TasmotaClient::applyConfig(c);
              MqttHa::applyConfig(c);
              req->send(200, "application/json", "{\"ok\":true}");
            });

  server.on("/api/control/apply", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    bool ok = HitControl::applyAll();
    JsonDocument out;
    out["ok"] = ok;
    out["error"] = HitControl::lastError();
    String s; serializeJson(out, s);
    req->send(200, "application/json", s);
  });

  server.on("/api/poll", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    HitClient::pollNow();
    TasmotaClient::pollNow();
    SunSpecServer::rebuildMap();
    MqttHa::publishNow();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/tasmota/probe", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t) {
              if (!requireAuth(req)) return;
              JsonDocument in;
              if (len > 0) deserializeJson(in, data, len);
              auto &c = ConfigStore::get();
              // temporär Host aus Request nutzen (ohne Speichern), dann zurück
              AppConfig tmp = c;
              if (in["tasmotaHost"].is<const char *>())
                strlcpy(tmp.tasmotaHost, in["tasmotaHost"], sizeof(tmp.tasmotaHost));
              if (!in["tasmotaPort"].isNull()) tmp.tasmotaPort = in["tasmotaPort"];
              if (in["tasmotaUser"].is<const char *>())
                strlcpy(tmp.tasmotaUser, in["tasmotaUser"], sizeof(tmp.tasmotaUser));
              if (in["tasmotaPass"].is<const char *>())
                strlcpy(tmp.tasmotaPass, in["tasmotaPass"], sizeof(tmp.tasmotaPass));
              tmp.tasmotaEnabled = true;
              TasmotaClient::applyConfig(tmp);

              String raw, summary;
              bool ok = TasmotaClient::probe(raw, summary);

              // Config wiederherstellen, Live-Werte bleiben
              TasmotaClient::applyConfig(c);
              if (ok) {
                c.tasmotaEnabled = true;
                strlcpy(c.tasmotaHost, tmp.tasmotaHost, sizeof(c.tasmotaHost));
                c.tasmotaPort = tmp.tasmotaPort;
                strlcpy(c.tasmotaUser, tmp.tasmotaUser, sizeof(c.tasmotaUser));
                strlcpy(c.tasmotaPass, tmp.tasmotaPass, sizeof(c.tasmotaPass));
                ConfigStore::save();
                TasmotaClient::applyConfig(c);
                SunSpecServer::rebuildMap();
              }

              JsonDocument out;
              out["ok"] = ok;
              out["summary"] = summary;
              out["raw"] = raw;
              out["powerW"] = gLive.tasmotaPowerW;
              out["e_in"] = gLive.tasmotaTotalKwh;
              out["e_out"] = gLive.tasmotaExportKwh;
              out["sensor"] = gLive.tasmotaSensorKey;
              String s; serializeJson(out, s);
              req->send(200, "application/json", s);
            });

  OtaUpdate::registerRoutes(server, requireAuth);

  server.begin();
}

void loop() {
  OtaUpdate::loop();
}

} // namespace WebConfig
