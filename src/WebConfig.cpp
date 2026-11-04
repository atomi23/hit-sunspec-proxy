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
#include "HoyCloud.h"
#include "KnxIp.h"
#include "KnxPoints.h"
#include "DataSource.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>

namespace {
AsyncWebServer server(80);

// Shared UI — sidebar + mint accents (all pages link /a.css)
const char UI_CSS[] PROGMEM = R"CSS(
:root,:root[data-theme=mint]{
  --bg:#0b0f0d;--side:#0c1210;--panel:#121a16;--line:rgba(88,204,134,.16);
  --txt:#e8f0eb;--muted:#7f9186;--acc:#58cc86;--acc2:#3dbf74;--warn:#f0b429;--danger:#ff6b6b;--cyan:#7dd3c0;
  --r:14px;--side-w:220px;--ease:cubic-bezier(.22,1,.36,1);
  --font:"Segoe UI Variable Text","Segoe UI","SF Pro Text",system-ui,sans-serif;
  --mono:ui-monospace,"Cascadia Mono","SF Mono",Consolas,monospace;
}
:root[data-theme=ocean]{
  --bg:#0a1016;--side:#0c141c;--panel:#121c26;--line:rgba(56,189,248,.18);
  --txt:#e8f2f8;--muted:#7e93a3;--acc:#38bdf8;--acc2:#0ea5e9;--warn:#fbbf24;--danger:#fb7185;--cyan:#67e8f9;
}
:root[data-theme=ember]{
  --bg:#120e0b;--side:#18120e;--panel:#1c1612;--line:rgba(251,146,60,.18);
  --txt:#f5efe8;--muted:#a08b7a;--acc:#fb923c;--acc2:#f97316;--warn:#facc15;--danger:#f87171;--cyan:#fdba74;
}
.side .prefs{display:grid;gap:6px;margin:10px 0 0}
.side .prefs select{padding:8px 10px;font-size:.78rem}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;min-height:100vh;font:15px/1.45 var(--font);color:var(--txt);background:var(--bg);letter-spacing:-.01em}
a{color:var(--acc)}
.app{display:flex;min-height:100vh;width:100%}
.side{width:var(--side-w);flex-shrink:0;background:var(--side);border-right:1px solid var(--line);
  padding:22px 14px;display:flex;flex-direction:column;position:sticky;top:0;height:100vh;z-index:5}
.logo{display:flex;gap:10px;align-items:center;padding:4px 8px 18px;border-bottom:1px solid var(--line);margin-bottom:14px}
.logo .mark{width:34px;height:34px;border-radius:10px;background:rgba(88,204,134,.14);border:1px solid rgba(88,204,134,.35);
  display:grid;place-items:center;color:var(--acc);font:700 .85rem/1 var(--font)}
.logo b{display:block;font-size:.78rem;letter-spacing:.02em}.logo span{display:block;font-size:.68rem;color:var(--acc);margin-top:2px}
.side nav{display:flex;flex-direction:column;gap:2px;flex:1;overflow:auto}
.side .nav-grp{margin:12px 8px 4px;font-size:.62rem;font-weight:700;letter-spacing:.1em;text-transform:uppercase;color:rgba(127,145,134,.85)}
.side .nav-grp:first-child{margin-top:0}
.side nav a{display:flex;align-items:center;gap:10px;padding:9px 12px;border-radius:10px;color:var(--muted);
  text-decoration:none;font-size:.86rem;font-weight:550;border:1px solid transparent;transition:.2s var(--ease)}
.side nav a:hover{color:var(--txt);background:rgba(255,255,255,.04)}
.side nav a.on{color:var(--acc);background:rgba(88,204,134,.1);border-color:rgba(88,204,134,.35)}
.side nav a i{width:20px;text-align:center;opacity:.9;font-style:normal;font-size:1rem;line-height:1}
.side .foot{margin-top:auto;padding:12px;border-radius:12px;border:1px solid var(--line);background:rgba(0,0,0,.25)}
.side .foot .online{display:flex;align-items:center;gap:8px;font-size:.8rem;font-weight:600;color:var(--acc)}
.side .foot .meta{margin-top:6px;font-size:.72rem;color:var(--muted)}
.main{flex:1;min-width:0;width:100%;padding:28px 36px 48px}
.page-head{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;align-items:flex-start;margin-bottom:20px}
.brand{font-size:.68rem;font-weight:650;letter-spacing:.12em;text-transform:uppercase;color:var(--acc);margin:0 0 6px}
h1{margin:0;font-size:clamp(1.35rem,2.6vw,1.75rem);font-weight:650;letter-spacing:-.03em;line-height:1.15}
.sub,.muted,.meta,.hint{color:var(--muted)}.sub{margin:6px 0 0;font-size:.92rem}
.panel,.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--r);padding:16px 18px;scroll-margin-top:24px}
.metrics{display:grid;gap:12px;grid-template-columns:repeat(4,1fr);margin:16px 0}
@media(max-width:1100px){.metrics{grid-template-columns:repeat(2,1fr)}}
.metric .k,.k{font-size:.68rem;font-weight:650;letter-spacing:.08em;text-transform:uppercase;color:var(--muted)}
.metric .v,.v{font:650 1.65rem/1.1 var(--mono);font-variant-numeric:tabular-nums;margin-top:8px;letter-spacing:-.02em}
.u{font:600 .8rem/1 var(--font);color:var(--muted);margin-left:2px}
.ip{font:650 clamp(1.25rem,3vw,1.7rem)/1.1 var(--mono);color:var(--cyan);letter-spacing:-.03em}
.badge{display:inline-block;margin-left:8px;padding:3px 9px;border-radius:8px;border:1px solid rgba(240,180,41,.35);
  background:rgba(240,180,41,.12);color:var(--warn);font:650 .68rem/1.2 var(--font);letter-spacing:.06em;vertical-align:middle}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin:14px 0 4px}
.chip{display:inline-flex;align-items:center;gap:7px;padding:6px 10px;border-radius:10px;border:1px solid var(--line);
  background:rgba(0,0,0,.28);color:var(--muted);font-size:.78rem;font-weight:550}
.dot{width:7px;height:7px;border-radius:50%;flex-shrink:0}
.dot.ok{background:var(--acc);box-shadow:0 0 8px rgba(88,204,134,.45)}.dot.bad{background:var(--danger)}
.flow{display:grid;grid-template-columns:1fr auto 1fr auto 1fr;gap:8px;align-items:stretch;margin:12px 0 16px}
.box{text-align:center;padding:14px 10px;border-radius:12px;border:1px solid var(--line);background:rgba(0,0,0,.28)}
.box span{display:block;font-size:.68rem;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);font-weight:650}
.box b{display:block;margin-top:8px;font:650 1.05rem/1 var(--mono);font-variant-numeric:tabular-nums}
.arr{color:rgba(88,204,134,.5);align-self:center;font-size:1.1rem}
.bar{height:8px;background:rgba(0,0,0,.4);border-radius:99px;overflow:hidden;margin-top:10px}
.fill{height:100%;width:0;border-radius:inherit;background:linear-gradient(90deg,var(--acc2),var(--acc));transition:width .5s var(--ease)}
.grid,.grid2{display:grid;gap:14px;width:100%}
.grid{grid-template-columns:repeat(2,minmax(0,1fr))}
.grid2{grid-template-columns:repeat(4,minmax(0,1fr));margin-top:12px}
@media(max-width:1200px){.grid2{grid-template-columns:repeat(2,1fr)}}
@media(max-width:900px){.grid,.grid2{grid-template-columns:1fr}}
.tile{display:flex;flex-direction:column;gap:12px;min-height:168px}
.tile-h{display:flex;align-items:center;justify-content:space-between;gap:10px}
.tile-h .k{margin:0}.tile-ico{width:34px;height:34px;border-radius:10px;display:grid;place-items:center;
  background:rgba(88,204,134,.12);border:1px solid rgba(88,204,134,.28);color:var(--acc);font-size:1rem}
.tile-row{display:flex;justify-content:space-between;align-items:baseline;gap:10px;padding:8px 0;border-bottom:1px solid var(--line)}
.tile-row:last-child{border-bottom:0;padding-bottom:0}.tile-row .lab{font-size:.78rem;color:var(--muted);font-weight:550}
.tile-row .num{font:650 1.05rem/1.1 var(--mono);font-variant-numeric:tabular-nums;color:var(--txt)}
.tile-row .num small{font:600 .72rem/1 var(--font);color:var(--muted);margin-left:4px}
.mppt{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:4px}
.mppt div{text-align:center;padding:8px 4px;border-radius:10px;background:rgba(0,0,0,.28);border:1px solid var(--line)}
.mppt b{display:block;font:650 .85rem/1.1 var(--mono);color:var(--acc);margin-top:4px}
.mppt span{font-size:.62rem;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);font-weight:650}
.stat-pill{display:inline-flex;align-items:center;gap:6px;padding:4px 9px;border-radius:999px;font:650 .68rem/1 var(--font);
  border:1px solid var(--line);background:rgba(0,0,0,.25);color:var(--muted)}
.stat-pill.on{color:var(--acc);border-color:rgba(88,204,134,.4);background:rgba(88,204,134,.1)}
.stat-pill.off{color:var(--danger);border-color:rgba(255,107,107,.35);background:rgba(255,107,107,.08)}
.card strong{display:block;font-size:.95rem;font-weight:650;letter-spacing:-.01em;margin-bottom:2px;color:var(--txt)}
label{display:block;font-size:.72rem;font-weight:650;letter-spacing:.04em;text-transform:uppercase;color:var(--muted);margin:14px 0 6px}
input,select{width:100%;padding:11px 12px;border-radius:10px;border:1px solid var(--line);background:rgba(0,0,0,.35);
  color:var(--txt);font:450 .95rem/1.3 var(--font);outline:none}
input:focus,select:focus{border-color:rgba(88,204,134,.5);box-shadow:0 0 0 3px rgba(88,204,134,.12)}
input[type=checkbox]{width:auto;margin-right:8px;accent-color:var(--acc)}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:18px;align-items:center}
button,.btn{appearance:none;border:0;cursor:pointer;padding:11px 16px;border-radius:10px;font:650 .9rem/1 var(--font);
  background:linear-gradient(180deg,#7be0a8,var(--acc));color:#062416}
button:hover,.btn:hover{filter:brightness(1.05)}button:active{transform:scale(.98)}
button.sec,.btn.sec{background:transparent;color:var(--txt);border:1px solid var(--line)}
.hint{font-size:.78rem;margin:8px 0 0;line-height:1.45}
.ok{color:var(--acc)}.bad{color:var(--danger)}
code{font-family:var(--mono);font-size:.86em;padding:2px 6px;border-radius:6px;background:rgba(0,0,0,.35);border:1px solid var(--line)}
pre{color:var(--muted);white-space:pre-wrap;font:450 .78rem/1.4 var(--mono);max-height:220px;overflow:auto;margin:8px 0 0}
.steps{display:flex;gap:0;margin:18px 0 8px;align-items:center}.steps i{flex:1;height:3px;border-radius:99px;background:rgba(255,255,255,.08)}
.steps i.on{background:var(--acc)}
.stepper{display:flex;gap:8px;flex-wrap:wrap;margin:16px 0 8px}.stepper span{font-size:.72rem;color:var(--muted);padding:4px 0;border-bottom:2px solid transparent}
.stepper span.on{color:var(--acc);border-color:var(--acc)}.stepper span.done{color:var(--acc)}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;border:1px solid rgba(88,204,134,.35);
  background:rgba(88,204,134,.1);color:var(--acc);font:650 .68rem/1.2 var(--font);letter-spacing:.04em;text-transform:uppercase}
.hide{display:none}ul{margin:10px 0 0 18px;color:var(--muted);font-size:.9rem;line-height:1.5}
.big{font:650 clamp(1.5rem,3.5vw,2rem)/1.1 var(--mono);color:var(--acc);letter-spacing:-.03em}
table{width:100%;border-collapse:collapse;font-size:.82rem}th,td{padding:10px 8px;border-bottom:1px solid var(--line);text-align:left}
th{color:var(--muted);font-weight:650;font-size:.7rem;letter-spacing:.06em;text-transform:uppercase}
.wizard{max-width:640px;margin:40px auto;padding:0 20px 48px}
#probeBox{margin-top:12px;display:none}#bar{height:8px;background:rgba(0,0,0,.4);border-radius:99px;overflow:hidden;margin-top:14px}
#fill{height:100%;width:0;background:linear-gradient(90deg,var(--acc2),var(--acc));transition:width .15s}
@media(max-width:860px){
  .app{flex-direction:column}.side{width:100%;height:auto;position:relative;border-right:0;border-bottom:1px solid var(--line)}
  .side nav{flex-direction:row;flex-wrap:wrap}.side .foot{display:none}.logo{border:0;margin:0;padding:0 4px 8px}
  .flow{grid-template-columns:1fr}.arr{display:none}
}
)CSS";

const char DASH_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Live · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><div class="mark">HS</div><div><b>HIT SunSpec</b><span>Proxy</span></div></div>
  <nav>
    <div class="nav-grp">System</div>
    <a class="on" href="/"><i>◉</i> Live</a>
    <div class="nav-grp">Anbindung</div>
    <a href="/config#hit"><i>⚡</i> HIT</a>
    <a href="/config#netz"><i>⊕</i> Netzwerk</a>
    <a href="/config#sunspec"><i>≈</i> SunSpec</a>
    <a href="/config#knx"><i>▦</i> KNX</a>
    <div class="nav-grp">Dienste</div>
    <a href="/config#mqtt"><i>☁</i> MQTT / Cloud</a>
    <a href="/config#tasmota"><i>⌀</i> Tasmota</a>
    <a href="/debug"><i>≡</i> Protokoll</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config"><i>⚙</i> Einstellungen</a>
    <a href="/ota"><i>↑</i> OTA</a>
  </nav>
  <div class="foot">
    <div class="online"><span class="dot ok"></span> Online</div>
    <div class="meta" id="sideVer">—</div>
    <div class="prefs">
      <select id="uiLang" onchange="saveUiPrefs()">
        <option value="de">Deutsch</option>
        <option value="en">English</option>
      </select>
      <select id="uiTheme" onchange="saveUiPrefs()">
        <option value="mint">Mint</option>
        <option value="ocean">Ocean</option>
        <option value="ember">Ember</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <p class="brand">System</p>
    <h1>Live <span id="ver" class="muted" style="font-size:.7rem;font-weight:500"></span></h1>
    <p class="sub">HIT → Proxy :1502 → Consolinno</p>
  </div>
  <div class="ip" id="ip">…</div>
</div>
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
  <div class="panel tile" id="en"></div>
  <div class="panel tile" id="lpp"></div>
  <div class="panel tile" id="tas"></div>
  <div class="panel tile" id="ph"></div>
</div>
<div class="row">
  <button class="sec" type="button" onclick="poll()">Jetzt pollen</button>
  <span class="meta" id="age"></span>
</div>
</main></div>

<script>
const I18N={
 de:{live:'Live',settings:'Einstellungen',proto:'Protokoll',energy:'Energiefluss',batSoc:'Batterie SoC',poll:'Jetzt pollen',
     pv:'PV',bat:'Batterie',grid:'Netz',soc:'SoC',enHit:'Energie HIT',yield:'PV Ertrag',imp:'Bezug',exp:'Einspeisung',
     lpp:'Consolinno LPP',limit:'Limit',on:'aktiv',off:'aus',hems:'HEMS',conn:'verbunden',wait:'wartet',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Phasen Netz',freq:'Frequenz',sys:'System'},
 en:{live:'Live',settings:'Settings',proto:'Log',energy:'Energy flow',batSoc:'Battery SoC',poll:'Poll now',
     pv:'PV',bat:'Battery',grid:'Grid',soc:'SoC',enHit:'HIT Energy',yield:'PV yield',imp:'Import',exp:'Export',
     lpp:'Consolinno LPP',limit:'Limit',on:'on',off:'off',hems:'HEMS',conn:'connected',wait:'waiting',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Grid phases',freq:'Frequency',sys:'System'}
};
let LANG='de';
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+' · '+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
const chip=(ok,l)=>`<span class="chip"><span class="dot ${ok?'ok':'bad'}"></span>${l}</span>`;
const w=n=>`${Number(n)||0} W`;
async function poll(){await fetch('/api/poll',{method:'POST'});refresh();}
async function refresh(){
  const s=await(await fetch('/api/status')).json();
  if(s.setupComplete===false){location.href='/setup';return;}
  applyThemeLang(s);
  document.getElementById('ver').textContent='v'+(s.version||'');
  document.getElementById('ip').innerHTML=(s.ipShown||'-')+(s.demoMode?' <span class="badge">DEMO</span>':'');
  document.getElementById('dots').innerHTML=
    chip(s.hitOk,'HIT')+chip(s.sunspecClient,'HEMS')+chip(s.mqttOk,'MQTT')+chip(s.mqttIngestOk,'MQ-IN')+chip(s.cloudOk,'CLOUD')+chip(s.knxOk,'KNX')+chip(s.ethOk||s.wifiOk,'NET')+
    `<span class="chip"><span class="dot ok"></span>SRC ${s.dataOrigin||'—'}</span>`;
  document.getElementById('m').innerHTML=
    `<div class="panel metric"><div class="tile-h"><div class="k">PV</div><div class="tile-ico">☀</div></div><div class="v">${s.pvW||0}<span class="u">W</span></div></div>
     <div class="panel metric"><div class="tile-h"><div class="k">Batterie</div><div class="tile-ico">▣</div></div><div class="v">${s.batW||0}<span class="u">W</span></div></div>
     <div class="panel metric"><div class="tile-h"><div class="k">Netz</div><div class="tile-ico">⚡</div></div><div class="v">${s.gridW||0}<span class="u">W</span></div></div>
     <div class="panel metric"><div class="tile-h"><div class="k">SoC</div><div class="tile-ico">●</div></div><div class="v">${s.soc||0}<span class="u">%</span></div></div>`;
  document.getElementById('fPv').textContent=w(s.pvW);
  document.getElementById('fBat').textContent=w(s.batW);
  document.getElementById('fGrid').textContent=w(s.gridW);
  const soc=Math.max(0,Math.min(100,Number(s.soc)||0));
  document.getElementById('socBar').style.width=soc+'%';
  document.getElementById('socTxt').textContent=soc+' % · SoH '+(s.soh||0)+'% · '+(Number(s.batV||0).toFixed(1))+' V · '+(Number(s.batA||0).toFixed(1))+' A · '+(s.status||'—');
  const epv=Number(s.energyPvKwh||0).toFixed(1), eimp=Number(s.energyImpKwh||0).toFixed(1), eexp=Number(s.energyExpKwh||0).toFixed(1);
  document.getElementById('en').innerHTML=`
    <div class="tile-h"><div class="k">Energie HIT</div><div class="tile-ico">☀</div></div>
    <div class="tile-row"><span class="lab">PV Ertrag</span><span class="num">${epv}<small>kWh</small></span></div>
    <div class="tile-row"><span class="lab">Bezug</span><span class="num">${eimp}<small>kWh</small></span></div>
    <div class="tile-row"><span class="lab">Einspeisung</span><span class="num">${eexp}<small>kWh</small></span></div>
    <div class="mppt">
      <div><span>MPPT1</span><b>${s.pv1W||0}</b></div>
      <div><span>MPPT2</span><b>${s.pv2W||0}</b></div>
      <div><span>MPPT3</span><b>${s.pv3W||0}</b></div>
      <div><span>MPPT4</span><b>${s.pv4W||0}</b></div>
    </div>`;
  const lpp=(Number(s.wMaxLimPct||0)/100).toFixed(1);
  document.getElementById('lpp').innerHTML=`
    <div class="tile-h"><div class="k">Consolinno LPP</div><div class="tile-ico">◎</div></div>
    <div class="tile-row"><span class="lab">WMaxLim</span><span class="num">${lpp}<small>%</small></span></div>
    <div class="tile-row"><span class="lab">Limit</span><span class="stat-pill ${s.wMaxLimEna?'on':'off'}">${s.wMaxLimEna?'aktiv':'aus'}</span></div>
    <div class="tile-row"><span class="lab">HEMS</span><span class="stat-pill ${s.sunspecClient?'on':'off'}">${s.sunspecClient?'verbunden':'wartet'}</span></div>
    <div class="tile-row"><span class="lab">KNX</span><span class="stat-pill ${s.knxOk?'on':'off'}">${s.knxOk?'ok':'—'}</span></div>
    <div class="meta">${s.lastWriteNote||'Kein Write bisher'}</div>`;
  document.getElementById('tas').innerHTML=s.tasmotaOk?`
    <div class="tile-h"><div class="k">Tasmota</div><div class="tile-ico">⌀</div></div>
    <div class="tile-row"><span class="lab">${s.tasmotaSensorKey||'Zähler'}</span><span class="num">${s.tasmotaPowerW||0}<small>W</small></span></div>
    <div class="tile-row"><span class="lab">E_in</span><span class="num">${Number(s.tasmotaTotalKwh||0).toFixed(2)}<small>kWh</small></span></div>
    <div class="tile-row"><span class="lab">E_out</span><span class="num">${Number(s.tasmotaExportKwh||0).toFixed(2)}<small>kWh</small></span></div>
    <span class="stat-pill on">online</span>`:`
    <div class="tile-h"><div class="k">Tasmota</div><div class="tile-ico">⌀</div></div>
    <div class="tile-row"><span class="lab">Status</span><span class="stat-pill off">offline</span></div>
    <div class="meta">Optional unter Dienste → Tasmota</div>`;
  document.getElementById('ph').innerHTML=`
    <div class="tile-h"><div class="k">Phasen Netz</div><div class="tile-ico">∿</div></div>
    <div class="tile-row"><span class="lab">L1</span><span class="num">${Number(s.v1||0).toFixed(1)}<small>V</small> · ${Number(s.i1||0).toFixed(1)}<small>A</small> · ${s.gridW1||0}<small>W</small></span></div>
    <div class="tile-row"><span class="lab">L2</span><span class="num">${Number(s.v2||0).toFixed(1)}<small>V</small> · ${Number(s.i2||0).toFixed(1)}<small>A</small> · ${s.gridW2||0}<small>W</small></span></div>
    <div class="tile-row"><span class="lab">L3</span><span class="num">${Number(s.v3||0).toFixed(1)}<small>V</small> · ${Number(s.i3||0).toFixed(1)}<small>A</small> · ${s.gridW3||0}<small>W</small></span></div>
    <div class="tile-row"><span class="lab">Frequenz</span><span class="num">${Number(s.freqHz||0).toFixed(2)}<small>Hz</small></span></div>`;
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
</head><body><div class="app">
<aside class="side">
  <div class="logo"><div class="mark">HS</div><div><b>HIT SunSpec</b><span>Proxy</span></div></div>
  <nav>
    <div class="nav-grp">System</div>
    <a href="/"><i>◉</i> Live</a>
    <div class="nav-grp">Anbindung</div>
    <a href="/config#hit"><i>⚡</i> HIT</a>
    <a href="/config#netz"><i>⊕</i> Netzwerk</a>
    <a href="/config#sunspec"><i>≈</i> SunSpec</a>
    <a href="/config#knx"><i>▦</i> KNX</a>
    <div class="nav-grp">Dienste</div>
    <a href="/config#mqtt"><i>☁</i> MQTT / Cloud</a>
    <a href="/config#tasmota"><i>⌀</i> Tasmota</a>
    <a href="/debug"><i>≡</i> Protokoll</a>
    <div class="nav-grp">Gerät</div>
    <a class="on" href="/config"><i>⚙</i> Einstellungen</a>
    <a href="/ota"><i>↑</i> OTA</a>
  </nav>
  <div class="foot">
    <div class="online"><span class="dot ok"></span> Online</div>
    <div class="meta" id="sideVer">—</div>
    <div class="prefs">
      <select id="uiLang" onchange="saveUiPrefs()">
        <option value="de">Deutsch</option>
        <option value="en">English</option>
      </select>
      <select id="uiTheme" onchange="saveUiPrefs()">
        <option value="mint">Mint</option>
        <option value="ocean">Ocean</option>
        <option value="ember">Ember</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <p class="brand">Konfiguration</p>
    <h1>Einstellungen</h1>
    <p class="sub">HIT Pflicht · Tasmota / MQTT / KNX optional</p>
  </div>
</div>
<div id="live" class="panel" style="margin-bottom:14px">Lade…</div>
<form id="f" class="grid">
<div class="card" id="hit"><strong>HIT Quelle</strong>
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
<div class="card" id="sunspec"><strong>SunSpec / Consolinno</strong>
<label>Listen-Port (Proxy)</label><input name="sunspecPort" type="number"/>
<label>Unit-ID</label><input name="sunspecUnitId" type="number"/>
<label><input name="invertPowerSign" type="checkbox"/> Leistung invertieren</label>
<label>Gerätename</label><input name="deviceName"/>
</div>
<div class="card" id="netz"><strong>Netzwerk & Display</strong>
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
<div class="card" id="ctrl"><strong>HIT Steuerung (Schreiben)</strong>
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
<div class="card" id="tasmota"><strong>Tasmota Zähler</strong>
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
<div class="card" id="mqtt"><strong>MQTT → Home Assistant</strong>
<label><input name="mqttEnabled" type="checkbox"/> Publish aktiv</label>
<label><input name="mqttDiscovery" type="checkbox"/> HA Discovery</label>
<label>Broker IP</label><input name="mqttHost" placeholder="192.0.2.10"/>
<label>Port</label><input name="mqttPort" type="number"/>
<label>User</label><input name="mqttUser"/>
<label>Passwort</label><input name="mqttPass" type="password"/>
<label>Topic-Basis</label><input name="mqttBase" placeholder="hitproxy"/>
<label>Publish (ms)</label><input name="mqttPublishMs" type="number"/>
<p class="hint">State: &lt;basis&gt;/state · inkl. data_origin</p>
<label><input name="mqttIngestEnabled" type="checkbox"/> <b>Ingest-Fallback</b> (wenn HIT tot)</label>
<label>Ingest-Topic (JSON)</label><input name="mqttIngestTopic" placeholder="hitproxy/in"/>
<label>Max Alter (ms)</label><input name="mqttIngestMaxAgeMs" type="number"/>
<p class="hint">JSON-Keys: pv_w, bat_w, grid_w, soc, energy_pv_kwh, … Priorität: NATIVE → MQTT → CLOUD</p>
</div>
<div class="card" id="cloud"><strong>Hoymiles Cloud Fallback</strong>
<label><input name="cloudEnabled" type="checkbox"/> aktiv (wenn HIT &amp; MQTT-in tot)</label>
<label>API Host</label><input name="cloudHost" placeholder="neapi.hoymiles.com"/>
<label>Cloud User / E-Mail</label><input name="cloudUser"/>
<label>Passwort</label><input name="cloudPass" type="password"/>
<label>Station / Plant ID (sid)</label><input name="cloudStationId" placeholder="z.B. 1234567"/>
<label>Poll (ms)</label><input name="cloudPollMs" type="number"/>
<label>Max Alter (ms)</label><input name="cloudMaxAgeMs" type="number"/>
<p class="hint">Inoffizielle S-Miles API — nur Lesen. Schreiben (§14a) nur über HIT Modbus.</p>
</div>
<div class="card" id="knx"><strong>KNX IP — Datenpunkte</strong>
<label><input name="knxEnabled" type="checkbox"/> aktiv (KNXnet/IP Routing Multicast 224.0.23.12:3671)</label>
<label>Physische Sendeadresse (Quelle im Telegramm)</label>
<input name="knxPa" placeholder="15.15.250" inputmode="decimal" autocomplete="off"/>
<p class="hint">Frei wählbar, z.B. <code>1.1.240</code> — erscheint als Absender-PA auf dem Bus (ETS/Monitor).</p>
<label>Intervall (ms)</label><input name="knxPublishMs" type="number"/>
<label><input name="knxOnlyOnChange" type="checkbox"/> nur bei Änderung</label>
<label>Min. Δ Leistung (W)</label><input name="knxMinDeltaW" type="number"/>
<p class="hint">Pro Zeile: aktivieren → <strong>Gruppenadresse manuell</strong> als Text <code>H/M/L</code> (z.B. <code>1/2/10</code>) → DPT ggf. anpassen.</p>
<div id="knxRows" style="margin-top:10px;display:grid;gap:8px"></div>
</div>
</form>
<div class="row">
<button type="button" onclick="save()">Speichern</button>
<button class="sec" type="button" onclick="poll()">Jetzt pollen</button>
<button class="sec" type="button" onclick="load()">Neu laden</button>
</div>
<p class="sub">Consolinno: Proxy-IP vom Display, Port 1502, Unit 1. HIT Pflicht · Tasmota/MQTT/KNX optional.</p>
</main></div>

<script>
const I18N={
 de:{live:'Live',settings:'Einstellungen',proto:'Protokoll',energy:'Energiefluss',batSoc:'Batterie SoC',poll:'Jetzt pollen',
     pv:'PV',bat:'Batterie',grid:'Netz',soc:'SoC',enHit:'Energie HIT',yield:'PV Ertrag',imp:'Bezug',exp:'Einspeisung',
     lpp:'Consolinno LPP',limit:'Limit',on:'aktiv',off:'aus',hems:'HEMS',conn:'verbunden',wait:'wartet',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Phasen Netz',freq:'Frequenz',sys:'System'},
 en:{live:'Live',settings:'Settings',proto:'Log',energy:'Energy flow',batSoc:'Battery SoC',poll:'Poll now',
     pv:'PV',bat:'Battery',grid:'Grid',soc:'SoC',enHit:'HIT Energy',yield:'PV yield',imp:'Import',exp:'Export',
     lpp:'Consolinno LPP',limit:'Limit',on:'on',off:'off',hems:'HEMS',conn:'connected',wait:'waiting',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Grid phases',freq:'Frequency',sys:'System'}
};
let LANG='de';
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+' · '+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
function dptOpts(kind,cur){
  const m={PowerW:[[14056,'14.056 W'],[9024,'9.024 kW']],Percent:[[5001,'5.001 %'],[9007,'9.007 %']],Volt:[[14027,'14.027 V'],[9001,'9.001']],Amp:[[14033,'14.033 A'],[9001,'9.001']],Hz:[[14035,'14.035 Hz'],[9001,'9.001']],EnergyKwh:[[13010,'13.010 Wh'],[14019,'14.019 kWh']],RatioPct:[[5001,'5.001 %'],[9007,'9.007 %']]};
  return (m[kind]||m.PowerW).map(([v,l])=>`<option value="${v}" ${Number(cur)===v?'selected':''}>${l}</option>`).join('');
}
function renderKnx(meta,ch){
  const box=document.getElementById('knxRows'); if(!box)return;
  box.innerHTML=meta.map((m,i)=>{
    const c=(ch&&ch[i])||{}; const dpt=c.dpt||m.dpt;
    return `<div style="display:grid;grid-template-columns:28px minmax(120px,1.2fr) minmax(100px,.9fr) minmax(120px,.9fr);gap:6px;align-items:center">
      <input type="checkbox" data-knx="e" data-i="${i}" ${c.ena?'checked':''} title="aktivieren"/>
      <span style="font-size:.82rem">${m.label}<br><span class="hint">${m.id}</span></span>
      <input data-knx="g" data-i="${i}" type="text" inputmode="text" autocomplete="off" placeholder="GA z.B. 1/2/${10+i}" value="${c.ga||''}" title="Gruppenadresse manuell H/M/L"/>
      <select data-knx="d" data-i="${i}" title="DPT">${dptOpts(m.kind,dpt)}</select></div>`;
  }).join('');
}
function collectKnx(){
  const n=document.querySelectorAll('#knxRows [data-knx=e]').length; const out=[];
  for(let i=0;i<n;i++){
    const e=document.querySelector(`#knxRows [data-knx=e][data-i="${i}"]`);
    const g=document.querySelector(`#knxRows [data-knx=g][data-i="${i}"]`);
    const d=document.querySelector(`#knxRows [data-knx=d][data-i="${i}"]`);
    out.push({ena:!!(e&&e.checked),ga:g?g.value.trim():'',dpt:d?Number(d.value):0});
  } return out;
}
function hashScroll(){const id=location.hash.slice(1);if(!id)return;const el=document.getElementById(id);if(!el)return;document.querySelectorAll('.card').forEach(c=>c.style.outline='');el.style.outline='1px solid rgba(88,204,134,.45)';el.scrollIntoView({behavior:'smooth',block:'start'});}
window.addEventListener('hashchange',hashScroll);
async function load(){
  const [c,meta]=await Promise.all([fetch('/api/config').then(r=>r.json()),fetch('/api/knx/meta').then(r=>r.json())]);
  const f=document.getElementById('f');
  for(const[k,v] of Object.entries(c)){if(k==='knxCh')continue;const el=f.elements.namedItem(k);if(!el)continue;if(el.type==='checkbox')el.checked=!!v;else el.value=v;}
  renderKnx(meta.points||[], c.knxCh||[]); refresh(); hashScroll();
}
async function save(){const f=document.getElementById('f');const body={};
[...f.elements].forEach(el=>{if(!el.name||el.dataset.knx)return;if(el.type==='checkbox')body[el.name]=el.checked;else if(el.type==='number')body[el.name]=Number(el.value);else body[el.name]=el.value;});
body.knxCh=collectKnx();
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
applyThemeLang(s);
const chip=(ok,l)=>`<span class="chip"><span class="dot ${ok?'ok':'bad'}"></span>${l}</span>`;
document.getElementById('live').innerHTML=`<div class="ip">${s.ipShown||'-'}${s.demoMode?' <span class="badge">DEMO</span>':''}</div>
<div class="chips">${chip(s.hitOk,'HIT')}${chip(s.sunspecClient,'HEMS')}${chip(s.mqttOk,'MQTT')}${chip(s.mqttIngestOk,'MQ-IN')}${chip(s.cloudOk,'CLOUD')}${chip(s.knxOk,'KNX')}${chip(s.ethOk||s.wifiOk,'NET')}<span class="chip"><span class="dot ok"></span>SRC ${s.dataOrigin||'—'}</span></div>
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
</head><body><div class="wizard">
<div class="logo" style="border:0;margin-bottom:8px;padding:0">
  <div class="mark">HS</div><div><b>HIT SunSpec</b><span>Proxy · Einrichtung</span></div>
</div>
<p class="brand">Assistent</p>
<h1>Einrichtung</h1>
<p class="sub">Hoymiles HIT → dieser Proxy → Consolinno</p>
<div class="stepper" id="stepLabels">
  <span>Willkommen</span><span>Zugang</span><span>HIT</span><span>Consolinno</span><span>Fertig</span>
</div>
<div class="steps" id="bar"><i></i><i></i><i></i><i></i><i></i></div>

<div class="card" id="s0">
<span class="pill">Schritt 1 / 5</span>
<strong style="margin-top:10px">Willkommen</strong>
<p class="hint">Nach der Einrichtung pollt der Proxy den HIT und stellt SunSpec auf Port <code>1502</code> bereit. Consolinno spricht nur noch diesen Proxy an.</p>
<ul>
<li>Display zeigt die Proxy-IP (Ethernet bevorzugt)</li>
<li>HA/MQTT optional — nicht nötig für Consolinno</li>
</ul>
<div class="row"><button type="button" onclick="go(1)">Weiter</button></div>
</div>

<div class="card hide" id="s1">
<span class="pill">Schritt 2 / 5</span>
<strong style="margin-top:10px">Zugang</strong>
<p class="hint">Display-Code (Login-Fallback, auf dem Display):</p>
<div class="ip" id="dcode">…</div>
<label>Web-User</label><input id="webUser" value="admin"/>
<label>Eigenes Web-Passwort (gemerkt)</label><input id="webPass" type="password" placeholder="empfohlen"/>
<p class="hint">Login danach: User + Passwort <b>oder</b> Display-Code.</p>
<div class="row"><button class="sec" type="button" onclick="go(0)">Zurück</button><button type="button" onclick="go(2)">Weiter</button></div>
</div>

<div class="card hide" id="s2">
<span class="pill">Schritt 3 / 5</span>
<strong style="margin-top:10px">Hoymiles HIT</strong>
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
<span class="pill">Schritt 4 / 5</span>
<strong style="margin-top:10px">Consolinno anbinden</strong>
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
<span class="pill">Schritt 5 / 5</span>
<strong style="margin-top:10px">Fertig — autark</strong>
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
  for(let i=0;i<5;i++){
    document.getElementById('s'+i).classList.toggle('hide',i!==step);
    document.querySelectorAll('#bar i')[i].classList.toggle('on',i<=step);
    const lab=document.querySelectorAll('#stepLabels span')[i];
    lab.classList.toggle('on',i===step); lab.classList.toggle('done',i<step);
  }
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
</head><body><div class="app">
<aside class="side">
  <div class="logo"><div class="mark">HS</div><div><b>HIT SunSpec</b><span>Proxy</span></div></div>
  <nav>
    <div class="nav-grp">System</div>
    <a href="/"><i>◉</i> Live</a>
    <div class="nav-grp">Anbindung</div>
    <a href="/config#hit"><i>⚡</i> HIT</a>
    <a href="/config#netz"><i>⊕</i> Netzwerk</a>
    <a href="/config#sunspec"><i>≈</i> SunSpec</a>
    <a href="/config#knx"><i>▦</i> KNX</a>
    <div class="nav-grp">Dienste</div>
    <a href="/config#mqtt"><i>☁</i> MQTT / Cloud</a>
    <a href="/config#tasmota"><i>⌀</i> Tasmota</a>
    <a class="on" href="/debug"><i>≡</i> Protokoll</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config"><i>⚙</i> Einstellungen</a>
    <a href="/ota"><i>↑</i> OTA</a>
  </nav>
  <div class="foot">
    <div class="online"><span class="dot ok"></span> Online</div>
    <div class="meta" id="sideVer">—</div>
    <div class="prefs">
      <select id="uiLang" onchange="saveUiPrefs()">
        <option value="de">Deutsch</option>
        <option value="en">English</option>
      </select>
      <select id="uiTheme" onchange="saveUiPrefs()">
        <option value="mint">Mint</option>
        <option value="ocean">Ocean</option>
        <option value="ember">Ember</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <p class="brand">Protokoll</p>
    <h1>§14a Write-Debug</h1>
    <p class="sub">SunSpec Model 123 Writes von Consolinno auf :1502</p>
  </div>
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
</main></div>

<script>
const I18N={
 de:{live:'Live',settings:'Einstellungen',proto:'Protokoll',energy:'Energiefluss',batSoc:'Batterie SoC',poll:'Jetzt pollen',
     pv:'PV',bat:'Batterie',grid:'Netz',soc:'SoC',enHit:'Energie HIT',yield:'PV Ertrag',imp:'Bezug',exp:'Einspeisung',
     lpp:'Consolinno LPP',limit:'Limit',on:'aktiv',off:'aus',hems:'HEMS',conn:'verbunden',wait:'wartet',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Phasen Netz',freq:'Frequenz',sys:'System'},
 en:{live:'Live',settings:'Settings',proto:'Log',energy:'Energy flow',batSoc:'Battery SoC',poll:'Poll now',
     pv:'PV',bat:'Battery',grid:'Grid',soc:'SoC',enHit:'HIT Energy',yield:'PV yield',imp:'Import',exp:'Export',
     lpp:'Consolinno LPP',limit:'Limit',on:'on',off:'off',hems:'HEMS',conn:'connected',wait:'waiting',tas:'Tasmota',
     online:'online',offline:'offline',ph:'Grid phases',freq:'Frequency',sys:'System'}
};
let LANG='de';
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+' · '+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
function age(ms){const s=Math.floor((Date.now()-window._boot-ms)/1000);if(s<0)return ms+'ms';if(s<60)return s+'s';return Math.floor(s/60)+'m';}
async function refresh(){
  const s=await(await fetch('/api/status')).json(); applyThemeLang(s);
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
  if (!o["mqttIngestEnabled"].isNull()) c.mqttIngestEnabled = o["mqttIngestEnabled"];
  if (o["mqttIngestTopic"].is<const char *>()) strlcpy(c.mqttIngestTopic, o["mqttIngestTopic"], sizeof(c.mqttIngestTopic));
  if (!o["mqttIngestMaxAgeMs"].isNull()) c.mqttIngestMaxAgeMs = o["mqttIngestMaxAgeMs"];
  if (!o["cloudEnabled"].isNull()) c.cloudEnabled = o["cloudEnabled"];
  if (o["cloudHost"].is<const char *>()) strlcpy(c.cloudHost, o["cloudHost"], sizeof(c.cloudHost));
  if (o["cloudUser"].is<const char *>()) strlcpy(c.cloudUser, o["cloudUser"], sizeof(c.cloudUser));
  if (o["cloudPass"].is<const char *>()) strlcpy(c.cloudPass, o["cloudPass"], sizeof(c.cloudPass));
  if (o["cloudStationId"].is<const char *>()) strlcpy(c.cloudStationId, o["cloudStationId"], sizeof(c.cloudStationId));
  if (!o["cloudPollMs"].isNull()) c.cloudPollMs = o["cloudPollMs"];
  if (!o["cloudMaxAgeMs"].isNull()) c.cloudMaxAgeMs = o["cloudMaxAgeMs"];
  if (!o["knxEnabled"].isNull()) c.knxEnabled = o["knxEnabled"];
  if (o["knxPa"].is<const char *>()) strlcpy(c.knxPa, o["knxPa"], sizeof(c.knxPa));
  if (!o["knxPublishMs"].isNull()) c.knxPublishMs = o["knxPublishMs"];
  if (!o["knxOnlyOnChange"].isNull()) c.knxOnlyOnChange = o["knxOnlyOnChange"];
  if (!o["knxMinDeltaW"].isNull()) c.knxMinDeltaW = o["knxMinDeltaW"];
  if (o["knxCh"].is<JsonArrayConst>()) {
    JsonArrayConst a = o["knxCh"].as<JsonArrayConst>();
    for (size_t i = 0; i < KNX_CH_COUNT && i < a.size(); i++) {
      if (!a[i]["ena"].isNull()) c.knxCh[i].ena = a[i]["ena"];
      if (a[i]["ga"].is<const char *>()) strlcpy(c.knxCh[i].ga, a[i]["ga"], sizeof(c.knxCh[i].ga));
      if (!a[i]["dpt"].isNull()) c.knxCh[i].dpt = a[i]["dpt"];
    }
  }
  if (o["deviceName"].is<const char *>()) strlcpy(c.deviceName, o["deviceName"], sizeof(c.deviceName));
  if (o["uiLang"].is<const char *>()) c.uiLang = uiLangFrom(o["uiLang"]);
  if (o["uiTheme"].is<const char *>()) c.uiTheme = uiThemeFrom(o["uiTheme"]);
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
              HoyCloud::applyConfig(c);
              KnxIp::applyConfig(c);
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
    doc["mqttIngestOk"] = gLive.mqttIngestOk;
    doc["cloudOk"] = gLive.cloudOk;
    doc["knxOk"] = gLive.knxOk;
    doc["dataOrigin"] = dataOriginLabel(gLive.dataOrigin);
    doc["pvW"] = gLive.pvW;
    doc["pv1W"] = gLive.pv1W; doc["pv2W"] = gLive.pv2W; doc["pv3W"] = gLive.pv3W; doc["pv4W"] = gLive.pv4W;
    doc["pv1V"] = gLive.pv1V; doc["pv2V"] = gLive.pv2V; doc["pv3V"] = gLive.pv3V; doc["pv4V"] = gLive.pv4V;
    doc["pv1A"] = gLive.pv1A; doc["pv2A"] = gLive.pv2A; doc["pv3A"] = gLive.pv3A; doc["pv4A"] = gLive.pv4A;
    doc["batW"] = gLive.batW;
    doc["batV"] = gLive.batV;
    doc["batA"] = gLive.batA;
    doc["soc"] = gLive.soc;
    doc["soh"] = gLive.soh;
    doc["gridW"] = gLive.gridW;
    doc["gridW1"] = gLive.gridW1; doc["gridW2"] = gLive.gridW2; doc["gridW3"] = gLive.gridW3;
    doc["v1"] = gLive.v1;
    doc["v2"] = gLive.v2;
    doc["v3"] = gLive.v3;
    doc["i1"] = gLive.i1;
    doc["i2"] = gLive.i2;
    doc["i3"] = gLive.i3;
    doc["freqHz"] = gLive.freqHz;
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
    doc["uiLang"] = uiLangCode(c.uiLang);
    doc["uiTheme"] = uiThemeCode(c.uiTheme);
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
    doc["mqttIngestEnabled"] = c.mqttIngestEnabled;
    doc["mqttIngestTopic"] = c.mqttIngestTopic;
    doc["mqttIngestMaxAgeMs"] = c.mqttIngestMaxAgeMs;
    doc["cloudEnabled"] = c.cloudEnabled;
    doc["cloudHost"] = c.cloudHost;
    doc["cloudUser"] = c.cloudUser;
    doc["cloudPass"] = c.cloudPass;
    doc["cloudStationId"] = c.cloudStationId;
    doc["cloudPollMs"] = c.cloudPollMs;
    doc["cloudMaxAgeMs"] = c.cloudMaxAgeMs;
    doc["knxEnabled"] = c.knxEnabled;
    doc["knxPa"] = c.knxPa;
    doc["knxPublishMs"] = c.knxPublishMs;
    doc["knxOnlyOnChange"] = c.knxOnlyOnChange;
    doc["knxMinDeltaW"] = c.knxMinDeltaW;
    JsonArray ch = doc["knxCh"].to<JsonArray>();
    for (size_t i = 0; i < KNX_CH_COUNT; i++) {
      JsonObject o = ch.add<JsonObject>();
      o["ena"] = c.knxCh[i].ena;
      o["ga"] = c.knxCh[i].ga;
      o["dpt"] = c.knxCh[i].dpt ? c.knxCh[i].dpt : knxMeta(i).defaultDpt;
    }
    doc["deviceName"] = c.deviceName;
    doc["uiLang"] = uiLangCode(c.uiLang);
    doc["uiTheme"] = uiThemeCode(c.uiTheme);
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/knx/meta", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    JsonArray pts = doc["points"].to<JsonArray>();
    static const char *kinds[] = {"PowerW", "Percent", "Volt", "Amp", "Hz", "EnergyKwh", "RatioPct"};
    for (size_t i = 0; i < KNX_CH_COUNT; i++) {
      const auto &m = knxMeta(i);
      JsonObject o = pts.add<JsonObject>();
      o["id"] = m.id;
      o["label"] = m.label;
      o["dpt"] = m.defaultDpt;
      o["kind"] = kinds[uint8_t(m.kind) < 7 ? uint8_t(m.kind) : 0];
    }
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
              HoyCloud::applyConfig(c);
              KnxIp::applyConfig(c);
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
    DataBroker::reconcile();
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
