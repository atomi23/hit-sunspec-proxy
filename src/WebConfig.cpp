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
#include "EventLog.h"
#include "HttpsServer.h"
#include "OtaUpdate.h"
#include "HoyCloud.h"
#include "KnxIp.h"
#include "KnxPoints.h"
#include "DataSource.h"
#include "UiFonts.h"
#include "Watchdog.h"
#include "History.h"
#include "HitBus.h"
#include "NetBoot.h"
#include "ModbusGateway.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace {
AsyncWebServer server(80);

// Shared UI — sidebar + mint accents (all pages link /a.css)
const char UI_CSS[] PROGMEM = R"CSS(
@font-face{font-family:Barlow;font-weight:400;font-display:swap;src:url(/f/barlow-400.woff) format("woff")}
@font-face{font-family:Barlow;font-weight:600;font-display:swap;src:url(/f/barlow-600.woff) format("woff")}
:root,:root[data-theme=mint]{
  --bg:#15191b;--side:#111416;--plate:#1d2225;--plate2:#232a2d;--well:#191d1f;--rule:#2c3337;--rule2:#3a4247;
  --ink:#edefec;--dim:#8f9893;--acc:#8ed8b4;--on-acc:#0d2419;
  --sun:#f1c453;--grid:#8ab8e6;--warn:#f0a35e;--fault:#ef6f62;--home:#c9b8f0;
  --font:Barlow,"Segoe UI",system-ui,sans-serif;
  --side-w:236px;--r:18px;--r-s:12px;
}
:root[data-theme=ocean]{
  --bg:#111821;--side:#0d131b;--plate:#18222d;--plate2:#1e2a37;--well:#141c25;--rule:#263443;--rule2:#33445a;
  --ink:#e8eef4;--dim:#8d9eb0;--acc:#79c7f2;--on-acc:#08202f;
}
:root[data-theme=ember]{
  --bg:#1b1615;--side:#161211;--plate:#241d1c;--plate2:#2c2422;--well:#1f1918;--rule:#382e2b;--rule2:#4a3d39;
  --ink:#f3ece6;--dim:#a39389;--acc:#f0a35e;--on-acc:#2e1806;--warn:#f1c453;
}
:root[data-theme=light]{
  color-scheme:light;
  --bg:#f2f3ef;--side:#ffffff;--plate:#ffffff;--plate2:#eceee9;--well:#f6f7f4;--rule:#dfe3dc;--rule2:#c9cfc6;
  --ink:#1a1f22;--dim:#69736c;--acc:#1f9d6b;--on-acc:#ffffff;
  --sun:#c98a00;--grid:#2f7fc4;--warn:#c76f12;--fault:#d14434;--home:#7a5bc7;
}
:root[data-theme=light] .panel,:root[data-theme=light] .card,:root[data-theme=light] .flow,:root[data-theme=light] .chip{
  box-shadow:0 1px 2px rgba(20,30,25,.06),0 4px 14px rgba(20,30,25,.05)}
:root[data-theme=light] .side{border-right:1px solid var(--rule)}
:root[data-theme=light] .side nav a:hover{background:rgba(0,0,0,.04)}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;min-height:100vh;font:400 15px/1.5 var(--font);color:var(--ink);background:var(--bg);
  font-variant-numeric:tabular-nums lining-nums;-webkit-font-smoothing:antialiased}
a{color:var(--acc)}
:focus-visible{outline:2px solid var(--acc);outline-offset:2px}

/* Seitengerüst */
.app{display:flex;min-height:100vh}
.side{width:var(--side-w);flex-shrink:0;background:var(--side);padding:22px 12px 16px;display:flex;flex-direction:column;
  position:sticky;top:0;height:100vh;z-index:5}
.logo{display:flex;gap:12px;align-items:center;padding:0 10px 22px}
.logo svg{width:34px;height:34px;flex-shrink:0;color:var(--acc)}
.logo b{display:block;font-weight:600;font-size:16px;line-height:1.1}
.logo span{display:block;font-size:12px;color:var(--dim);margin-top:3px}
.side nav{display:flex;flex-direction:column;gap:2px;flex:1;overflow:auto}
.side .nav-grp{margin:18px 12px 6px;font-size:12px;color:var(--dim)}
.side nav a{display:flex;align-items:center;gap:12px;padding:8px 12px;border-radius:10px;color:var(--dim);
  text-decoration:none;font-size:15px;transition:background .15s,color .15s}
.side nav a:hover{color:var(--ink);background:rgba(255,255,255,.04)}
.side nav a.on{color:var(--ink);background:var(--plate2)}
.side nav a svg{width:18px;height:18px;flex-shrink:0}
.side nav a.on svg{color:var(--acc)}
.side .foot{margin-top:auto;padding:14px 10px 0}
.side .foot .online{display:flex;align-items:center;gap:8px;font-size:13px}
.side .foot .meta{margin-top:4px;font-size:12px;color:var(--dim);white-space:pre-line}
.side .prefs{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:12px}
.side .prefs select{padding:7px 9px;font-size:13px;border-radius:10px}
.main{flex:1;min-width:0;width:100%;padding:20px clamp(12px,1.6vw,28px) 28px;display:flex;flex-direction:column}
.page-head{display:flex;justify-content:space-between;gap:16px;flex-wrap:wrap;align-items:flex-end;margin-bottom:14px;flex-shrink:0}
.brand{display:none}
h1{margin:0;font-size:32px;font-weight:600;line-height:1.1;letter-spacing:-.01em}
h2{margin:0 0 12px;font-size:17px;font-weight:600}
.sub{margin:8px 0 0;font-size:15px;color:var(--dim);max-width:68ch}
.muted,.meta,.hint{color:var(--dim)}
.ip{font-size:17px;color:var(--ink);text-align:right}
.ip small{display:block;font-size:13px;color:var(--dim)}
.tag{display:inline-block;margin-left:12px;padding:3px 10px;border-radius:999px;font-size:13px;font-weight:600;
  color:var(--warn);background:color-mix(in srgb,var(--warn) 14%,transparent);vertical-align:6px}

/* Flächen */
.panel,.card{background:var(--plate);border-radius:var(--r);padding:22px 24px}
.card>strong{display:block;font-size:17px;font-weight:600;margin-bottom:4px}
.grid,.grid2{display:grid;gap:16px}
.grid{grid-template-columns:repeat(2,minmax(0,1fr))}
.grid2{grid-template-columns:repeat(4,minmax(0,1fr));margin-top:12px}

/* Formulare */
label{display:block;font-size:13px;color:var(--dim);margin:16px 0 6px}
label:has(input[type=checkbox]){display:flex;align-items:center;gap:10px;color:var(--ink);font-size:15px}
input,select{width:100%;padding:10px 12px;border-radius:10px;border:1px solid var(--rule);background:var(--well);
  color:var(--ink);font:400 15px/1.3 var(--font);font-variant-numeric:tabular-nums;transition:border-color .15s}
input:hover,select:hover{border-color:var(--rule2)}
input:focus,select:focus{border-color:var(--acc);outline:none;box-shadow:0 0 0 3px color-mix(in srgb,var(--acc) 22%,transparent)}
input[readonly]{color:var(--dim)}
input[type=checkbox]{width:18px;height:18px;margin:0;accent-color:var(--acc)}
.row{display:flex;gap:10px;flex-wrap:wrap;margin-top:22px;align-items:center}
button,.btn{appearance:none;cursor:pointer;padding:10px 18px;border-radius:999px;border:0;
  font:600 15px/1.2 var(--font);background:var(--acc);color:var(--on-acc);transition:filter .15s,background .15s}
button:hover,.btn:hover{filter:brightness(1.08)}
button.sec,.btn.sec{background:var(--plate2);color:var(--ink)}
button.sec:hover{background:var(--rule)}
.hint{font-size:13px;margin:8px 0 0;line-height:1.5;max-width:68ch}
.ok{color:var(--acc)}.bad{color:var(--fault)}
code{font:400 13px ui-monospace,Consolas,monospace;padding:2px 6px;border-radius:6px;background:var(--well)}
pre{color:var(--dim);white-space:pre-wrap;font:400 13px/1.5 ui-monospace,Consolas,monospace;max-height:260px;overflow:auto;margin:10px 0 0}
table{width:100%;border-collapse:collapse;font-size:15px}
th,td{padding:9px 0;border-bottom:1px solid var(--rule);text-align:left}
tr:last-child td{border-bottom:0}
th{color:var(--dim);font-weight:400;font-size:13px}
td.n,th.n{text-align:right}
ul{margin:10px 0 0 18px;color:var(--dim);line-height:1.5}
.hide{display:none}
.big{font-size:30px;color:var(--acc)}
.k{font-size:13px;color:var(--dim)}
.v{font-size:30px;line-height:1.1;margin-top:4px}
.u{font-size:15px;color:var(--dim);margin-left:3px}

/* Status-Pillen */
.chips{display:flex;gap:8px;flex-wrap:wrap;margin:0}
.chip{display:inline-flex;align-items:center;gap:8px;padding:6px 12px 6px 10px;border-radius:999px;background:var(--plate);
  font-size:13px;color:var(--dim)}
.chip.on{color:var(--ink)}
.dot{width:8px;height:8px;border-radius:50%;flex-shrink:0;background:var(--rule2)}
.dot.ok,.chip.on .dot{background:var(--acc);box-shadow:0 0 0 3px color-mix(in srgb,var(--acc) 22%,transparent)}
.dot.bad,.chip.off .dot{background:var(--fault)}
.chip.idle .dot{background:var(--rule2)}
.stat-pill,.pill{display:inline-block;padding:2px 10px;border-radius:999px;background:var(--plate2);font-size:13px;color:var(--dim)}
.stat-pill.on,.pill{color:var(--acc)}
.stat-pill.off{color:var(--dim)}
.bar,#bar{height:8px;background:var(--well);border-radius:999px;overflow:hidden;margin-top:12px}
.fill,#fill{height:100%;width:0;background:var(--acc);border-radius:999px;transition:width .3s}
.steps{display:flex;gap:6px;margin:18px 0 8px}.steps i{flex:1;height:4px;border-radius:999px;background:var(--rule)}.steps i.on{background:var(--acc)}
.stepper{display:flex;gap:8px;flex-wrap:wrap;margin:16px 0 8px}
.stepper span{font-size:13px;color:var(--dim);padding:5px 12px;border-radius:999px;background:var(--plate)}
.stepper span.on{color:var(--on-acc);background:var(--acc)}.stepper span.done{color:var(--acc)}
.wizard{max-width:620px;margin:40px auto;padding:0 20px 48px}
#probeBox{margin-top:12px;display:none}

/* Live: Energiefluss */
.flow{background:radial-gradient(120% 90% at 50% 0%,var(--plate2),var(--plate) 70%);border-radius:24px;padding:8px 8px 2px}
.flow svg{display:block;width:100%;height:auto;max-height:58vh;overflow:visible}
.flow .lbl{fill:var(--dim);font:400 15px var(--font)}
.flow .val{fill:var(--ink);font:600 36px var(--font);letter-spacing:-.01em}
.flow .val tspan{font-size:17px;font-weight:400;fill:var(--dim)}
.flow .dir{fill:var(--dim);font:400 13px var(--font)}
.flow .track{fill:none;stroke:var(--rule);stroke-width:2}
.flow .run{fill:none;stroke-width:5;stroke-linecap:round;stroke-dasharray:.1 22;opacity:0;transition:opacity .4s}
.flow .run.on{opacity:1;animation:flow 1.2s linear infinite}
.flow .run.rev{animation-direction:reverse}
.flow .c-pv{stroke:var(--sun);filter:drop-shadow(0 0 4px var(--sun))}
.flow .c-bt{stroke:var(--acc);filter:drop-shadow(0 0 4px var(--acc))}
.flow .c-gr{stroke:var(--grid);filter:drop-shadow(0 0 4px var(--grid))}
.flow .c-ho{stroke:var(--home);filter:drop-shadow(0 0 4px var(--home))}
.flow .node{stroke-width:1.5}
.flow .n-pv{fill:color-mix(in srgb,var(--sun) 12%,var(--plate));stroke:color-mix(in srgb,var(--sun) 45%,transparent)}
.flow .n-bt{fill:color-mix(in srgb,var(--acc) 12%,var(--plate));stroke:color-mix(in srgb,var(--acc) 30%,transparent)}
.flow .n-gr{fill:color-mix(in srgb,var(--grid) 12%,var(--plate));stroke:color-mix(in srgb,var(--grid) 45%,transparent)}
.flow .n-ho{fill:color-mix(in srgb,var(--home) 12%,var(--plate));stroke:color-mix(in srgb,var(--home) 45%,transparent)}
.flow .ico{fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}
.flow .i-pv{stroke:var(--sun)}.flow .i-bt{stroke:var(--acc)}.flow .i-gr{stroke:var(--grid)}.flow .i-ho{stroke:var(--home)}
.flow .hub{fill:var(--well);stroke:var(--rule2);stroke-width:1.5}
.flow .hub-i{fill:none;stroke:var(--ink);stroke-width:2;stroke-linecap:round}
.flow .ring-bg{fill:none;stroke:var(--rule);stroke-width:4}
.flow .ring{fill:none;stroke:var(--acc);stroke-width:4;stroke-linecap:round;transition:stroke-dasharray .6s}
.flow .chipbox{fill:var(--well);stroke:var(--rule2);stroke-width:1.5}
.flow .chipbox.live{stroke:var(--acc)}
.flow .data{fill:none;stroke:var(--rule2);stroke-width:1.5;stroke-dasharray:4 6}
.flow .data.live{stroke:var(--acc);animation:flow 1.6s linear infinite reverse}
@keyframes flow{to{stroke-dashoffset:-22.1}}
@media (prefers-reduced-motion:reduce){.flow .run.on,.flow .data.live{animation:none}}

/* Live: Kennzahlen */
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:12px;margin-top:12px;flex-shrink:0}
.cards.two{grid-template-columns:repeat(auto-fit,minmax(420px,1fr))}
.cards .panel{padding:20px 22px 14px}
.cards td:first-child{color:var(--dim)}
.note{font-size:13px;color:var(--dim);margin:10px 0 6px}
.foot-row{display:flex;gap:16px;align-items:center;flex-wrap:wrap;margin-top:24px}

/* Einstellungen: ein Bereich je Menüpunkt */
.cfg .card.show{columns:2 380px;column-gap:48px}
.cfg .card label{break-after:avoid}
.cfg .card input,.cfg .card select,.cfg .card .hint,.cfg .card button{break-inside:avoid}
.cfg #knxRows,.cfg #probeBox,.cfg #tlsBox{column-span:all}
textarea{width:100%;padding:10px 12px;border-radius:10px;border:1px solid var(--rule);background:var(--well);color:var(--ink);
  font:400 12px/1.4 ui-monospace,Consolas,monospace;resize:vertical}
textarea:focus{border-color:var(--acc);outline:none}
#tlsBox input[type=file]{margin-top:6px;padding:6px;font-size:13px}
#tlsInfo td:first-child{color:var(--dim);width:150px}
#tlsInfo td{word-break:break-all}
.cfg .card{display:none}
.cfg .card.show{display:block}
.cfg .card>strong{display:none}
.cfg .card>label:first-of-type{margin-top:0}

@media(max-width:1100px){.grid2{grid-template-columns:repeat(2,1fr)}}
@media(max-width:860px){
  .app{flex-direction:column}
  .side{width:100%;height:auto;position:relative;padding:14px 10px 8px}
  .side nav{flex-direction:row;flex-wrap:wrap;gap:4px}.side nav a{padding:6px 10px}
  .side .nav-grp{display:none}.side .foot{display:none}.logo{padding:0 8px 10px}
  .main{padding:20px 16px 40px}.grid,.grid2,.cards,.cards.two{grid-template-columns:1fr}
  .flow{overflow-x:auto}.flow svg{min-width:640px}
}
)CSS";

const char DASH_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Live · HIT Proxy</title>
<link rel="preload" href="/f/barlow-600.woff" as="font" type="font/woff" crossorigin/>
<link rel="stylesheet" href="/a.css"/>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/><path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg><div><b>HIT-Proxy</b><span>SunSpec für Consolinno</span></div></div>
  <nav>
    <a class="on" aria-current="page" href="/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l2-6 4 12 2-6h6"/></svg> Live</a>
    <a href="/history"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 19V5M4 19h16M8 15l3-4 3 2 4-6"/></svg> Verlauf</a>
    <div class="nav-grp">Datenquellen</div>
    <a href="/config#hit"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/></svg> HIT-Wechselrichter</a>
    <a href="/config#tasmota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/></svg> Tasmota-Zähler</a>
    <a href="/config#cloud"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/></svg> Hoymiles-Cloud</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#netz"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/></svg> Netzwerk und Zugang</a>
    <a href="/config#display"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/></svg> Display</a>
    <a href="/config#ctrl"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></svg> Akku-Steuerung</a>
    <a href="/debug"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/></svg> Protokoll</a>
    <a href="/ota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/></svg> Firmware und Reset</a>
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
        <option value="light">Hell</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <h1><span data-t="live">Live</span><span id="demo" class="tag hide" data-t="demo">Demo-Daten</span></h1>
    <div class="chips" id="chips" style="margin-top:14px"></div>
  </div>
  <div class="ip"><span id="ip">…</span><small data-t="port">Modbus-Port 1502</small></div>
</div>

<div class="flow" role="img" aria-labelledby="flowDesc">
<p id="flowDesc" class="hide"></p>
<svg viewBox="0 0 960 400" xmlns="http://www.w3.org/2000/svg">
  <path class="track" d="M344 110C400 110 400 192 436 192"/>
  <path class="track" d="M344 310C400 310 400 228 436 228"/>
  <path class="track" d="M524 192C560 192 560 110 616 110"/>
  <path class="track" d="M524 228C560 228 560 310 616 310"/>
  <path id="fPv" class="run c-pv" d="M344 110C400 110 400 192 436 192"/>
  <path id="fBat" class="run c-bt" d="M344 310C400 310 400 228 436 228"/>
  <path id="fGrid" class="run c-gr" d="M524 192C560 192 560 110 616 110"/>
  <path id="fHouse" class="run c-ho" d="M524 228C560 228 560 310 616 310"/>
  <path id="wData" class="data" d="M480 64V166"/>

  <rect id="hemsBox" class="chipbox" x="370" y="24" width="220" height="40" rx="20"/>
  <text x="480" y="49" text-anchor="middle" class="lbl"><tspan style="fill:var(--ink)">Consolinno</tspan><tspan dx="8" id="hemsTxt">wartet</tspan></text>

  <rect class="hub" x="436" y="166" width="88" height="88" rx="26"/>
  <path class="hub-i" d="M462 194h36M462 201h36" style="stroke-dasharray:6 4"/>
  <path class="hub-i" d="M462 224c4.5-8 7.5-8 12 0s7.5 8 12 0 7.5-8 12 0"/>
  <text x="480" y="284" text-anchor="middle" class="lbl" style="fill:var(--ink)">Hoymiles HIT</text>
  <text x="480" y="304" text-anchor="middle" class="dir" id="dHit">—</text>

  <circle class="node n-pv" cx="300" cy="110" r="44"/>
  <g class="ico i-pv" transform="translate(283 93) scale(1.42)"><circle cx="12" cy="12" r="4"/><path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6 7 7M17 17l1.4 1.4M5.6 18.4 7 17M17 7l1.4-1.4"/></g>

  <circle class="ring-bg" cx="300" cy="310" r="52"/>
  <circle id="socRing" class="ring" cx="300" cy="310" r="52" transform="rotate(-90 300 310)" stroke-dasharray="0 327"/>
  <circle class="node n-bt" cx="300" cy="310" r="44"/>
  <g class="ico i-bt" transform="translate(283 293) scale(1.42)"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></g>

  <circle class="node n-gr" cx="660" cy="110" r="44"/>
  <g class="ico i-gr" transform="translate(643 93) scale(1.42)"><path d="M8 21 12 3l4 18M9.6 14h4.8M10.6 9h2.8M5 7h14"/></g>

  <circle class="node n-ho" cx="660" cy="310" r="44"/>
  <g class="ico i-ho" transform="translate(643 293) scale(1.42)"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></g>

  <text x="228" y="96" text-anchor="end" class="lbl" data-t="pv">PV-Leistung</text>
  <text x="228" y="134" text-anchor="end" class="val" id="vPv">—</text>
  <text x="228" y="158" text-anchor="end" class="dir" id="dPv"></text>

  <text x="228" y="296" text-anchor="end" class="lbl" data-t="bat">Akku</text>
  <text x="228" y="334" text-anchor="end" class="val" id="vBat">—</text>
  <text x="228" y="358" text-anchor="end" class="dir" id="dBat"></text>

  <text x="732" y="96" class="lbl" data-t="grid">Netz</text>
  <text x="732" y="134" class="val" id="vGrid">—</text>
  <text x="732" y="158" class="dir" id="dGrid"></text>

  <text x="732" y="296" class="lbl" data-t="house">Haus</text>
  <text x="732" y="334" class="val" id="vHouse">—</text>
  <text x="732" y="358" class="dir" data-t="calc">aus PV, Akku und Netz</text>
</svg>
</div>

<div class="cards">
  <section class="panel">
    <h2 data-t="strings">PV-Strings</h2>
    <table id="tStr"></table>
  </section>
  <section class="panel">
    <h2 data-t="phases">Netz je Phase</h2>
    <table id="tPh"></table>
    <p class="note" id="nHz"></p>
  </section>
  <section class="panel">
    <h2 data-t="meters">Zähler und Akku</h2>
    <table id="tEn"></table>
  </section>
</div>
<div class="cards two">
  <section class="panel">
    <h2 data-t="limit">Leistungsbegrenzung durch Consolinno</h2>
    <table id="tLim"></table>
    <p class="note" id="nLim"></p>
  </section>
  <section class="panel">
    <h2 data-t="tas">Tasmota-Zähler</h2>
    <table id="tTas"></table>
    <p class="note" id="nTas"></p>
  </section>
</div>

<div class="foot-row">
  <button class="sec" type="button" onclick="poll()" data-t="poll">HIT jetzt abfragen</button>
  <span class="meta" id="age"></span>
</div>
</main></div>

<script>
const I18N={
 de:{live:'Live',settings:'Einstellungen',proto:'Protokoll'},
 en:{live:'Live',settings:'Settings',proto:'Log'}
};
let LANG='de';
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
const TX={
 de:{live:'Live',demo:'Demo-Daten',port:'Modbus-Port 1502',pv:'PV-Leistung',bat:'Akku',grid:'Netz',house:'Haus',
   calc:'aus PV, Akku und Netz',strings:'PV-Strings',phases:'Netz je Phase',meters:'Zähler und Akku',
   limit:'Leistungsbegrenzung durch Consolinno',tas:'Tasmota-Zähler',poll:'HIT jetzt abfragen',
   charge:'lädt',discharge:'entlädt',idle:'ruht',imp:'Bezug',exp:'Einspeisung',soc:'Ladestand',
   hitOk:'antwortet',hitBad:'keine Antwort',hemsOk:'fragt ab',hemsWait:'wartet',
   string:'String',total:'Summe',phase:'Phase',freq:'Netzfrequenz',yield:'PV-Ertrag gesamt',eimp:'Netzbezug gesamt',
   eexp:'Einspeisung gesamt',batv:'Akkuspannung',bata:'Akkustrom',soh:'Akkuzustand (SoH)',
   lim:'Begrenzung',limOn:'aktiv',limOff:'keine',limVal:'Grenzwert',noWrite:'Consolinno hat noch nichts geschrieben.',
   tasOff:'Kein Tasmota-Zähler verbunden. Einrichten unter Datenquellen, Tasmota-Zähler.',power:'Leistung',
   src:'Quelle',srcN:'HIT direkt',srcM:'MQTT',srcC:'Hoymiles-Cloud',srcD:'Demo',srcX:'keine',
   age:s=>'Letzte Antwort vom HIT vor '+s+' s',net:'Netzwerk',hems:'Consolinno',mqin:'MQTT-Eingang',
   today:'Ertrag gesamt',flow:'Energiefluss zwischen PV, Akku, HIT, Netz und Haus'},
 en:{live:'Live',demo:'Demo data',port:'Modbus port 1502',pv:'Solar power',bat:'Battery',grid:'Grid',house:'Home',
   calc:'from solar, battery and grid',strings:'PV strings',phases:'Grid per phase',meters:'Meters and battery',
   limit:'Power limit set by Consolinno',tas:'Tasmota meter',poll:'Poll HIT now',
   charge:'charging',discharge:'discharging',idle:'idle',imp:'import',exp:'export',soc:'Charge',
   hitOk:'responding',hitBad:'no response',hemsOk:'polling',hemsWait:'waiting',
   string:'String',total:'Total',phase:'Phase',freq:'Grid frequency',yield:'PV yield total',eimp:'Grid import total',
   eexp:'Export total',batv:'Battery voltage',bata:'Battery current',soh:'Battery health (SoH)',
   lim:'Limit',limOn:'active',limOff:'none',limVal:'Setpoint',noWrite:'Consolinno has not written anything yet.',
   tasOff:'No Tasmota meter connected. Set it up under Data sources, Tasmota meter.',power:'Power',
   src:'Source',srcN:'HIT direct',srcM:'MQTT',srcC:'Hoymiles cloud',srcD:'Demo',srcX:'none',
   age:s=>'Last HIT response '+s+' s ago',net:'Network',hems:'Consolinno',mqin:'MQTT input',
   today:'Total yield',flow:'Energy flow between solar, battery, HIT, grid and home'}
};
const T=k=>(TX[LANG]||TX.de)[k];
const $=id=>document.getElementById(id);
const num=(n,d=0)=>(Number(n)||0).toLocaleString(LANG==='en'?'en-US':'de-DE',{minimumFractionDigits:d,maximumFractionDigits:d});
const watt=n=>num(Math.abs(n))+'<tspan dx="6">W</tspan>';
const rows=r=>r.map(([k,v])=>`<tr><td>${k}</td><td class="n">${v}</td></tr>`).join('');
function run(id,w,forward){const e=$(id);const on=Math.abs(w)>20;e.classList.toggle('on',on);e.classList.toggle('rev',on&&!forward);}
const chip=(state,label)=>`<span class="chip ${state}"><span class="dot"></span>${label}</span>`;
async function poll(){await fetch('/api/poll',{method:'POST'});refresh();}
async function refresh(){
  const s=await(await fetch('/api/status')).json();
  if(s.setupComplete===false){location.href='/setup';return;}
  applyThemeLang(s);
  document.querySelectorAll('[data-t]').forEach(e=>{const v=T(e.dataset.t);if(typeof v==='string')e.textContent=v;});
  $('flowDesc').textContent=T('flow');
  $('ip').textContent=s.ipShown||'—';
  $('demo').classList.toggle('hide',!s.demoMode);

  const pv=Number(s.pvW)||0, bat=Number(s.batW)||0, grid=Number(s.gridW)||0;
  const house=Math.max(0,pv+bat+grid);
  $('vPv').innerHTML=watt(pv);
  $('dPv').textContent=T('today')+' '+num(s.energyPvKwh)+' kWh';
  $('vBat').innerHTML=watt(bat);
  const soc=Math.max(0,Math.min(100,Number(s.soc)||0));
  $('dBat').textContent=(bat>20?T('discharge'):(bat<-20?T('charge'):T('idle')))+', '+num(soc)+' %';
  $('socRing').setAttribute('stroke-dasharray',(326.7*soc/100).toFixed(1)+' 327');
  $('socRing').style.stroke=soc<15?'var(--fault)':(soc<30?'var(--warn)':'var(--acc)');
  $('vGrid').innerHTML=watt(grid);
  $('dGrid').textContent=grid>20?T('imp'):(grid<-20?T('exp'):'');
  $('vHouse').innerHTML=watt(house);
  run('fPv',pv,true); run('fBat',bat,bat>0); run('fGrid',grid,grid<0); run('fHouse',house,true);
  $('dHit').textContent=s.hitOk?T('hitOk'):T('hitBad');
  $('dHit').style.fill=s.hitOk?'var(--dim)':'var(--fault)';
  $('hemsTxt').textContent=s.sunspecClient?T('hemsOk'):T('hemsWait');
  $('wData').classList.toggle('live',!!s.sunspecClient);
  $('hemsBox').classList.toggle('live',!!s.sunspecClient);

  const src={NATIVE:T('srcN'),MQTT:T('srcM'),CLOUD:T('srcC'),DEMO:T('srcD')}[s.dataOrigin]||T('srcX');
  const opt=ok=>ok?'on':'idle';
  $('chips').innerHTML=chip(s.hitOk?'on':'off','HIT')+chip(opt(s.sunspecClient),T('hems'))+
    chip((s.ethOk||s.wifiOk)?'on':'off',T('net'))+chip(opt(s.mqttOk),'MQTT')+chip(opt(s.mqttIngestOk),T('mqin'))+
    chip(opt(s.cloudOk),'Cloud')+chip(opt(s.knxOk),'KNX')+
    (s.gatewayEnabled?chip(opt(s.gatewayClients>0),'Gateway'+(s.gatewayClients?' ('+s.gatewayClients+')':'')):'')+
    `<span class="chip">${T('src')}: <b style="color:var(--ink);font-weight:600">${src}</b></span>`;

  const str=[1,2,3,4].map(i=>[T('string')+' '+i,num(s['pv'+i+'W'])+' W']);
  str.push([T('total'),`<b>${num(pv)} W</b>`]);
  $('tStr').innerHTML=rows(str);
  $('tPh').innerHTML=`<tr><th>${T('phase')}</th><th class="n">V</th><th class="n">A</th><th class="n">W</th></tr>`+
    [1,2,3].map(i=>`<tr><td>L${i}</td><td class="n">${num(s['v'+i],1)}</td><td class="n">${num(s['i'+i],1)}</td><td class="n">${num(s['gridW'+i])}</td></tr>`).join('');
  $('nHz').textContent=T('freq')+' '+num(s.freqHz,2)+' Hz';
  $('tEn').innerHTML=rows([
    [T('yield'),num(s.energyPvKwh,1)+' kWh'],[T('eimp'),num(s.energyImpKwh,1)+' kWh'],[T('eexp'),num(s.energyExpKwh,1)+' kWh'],
    [T('batv'),num(s.batV,1)+' V'],[T('bata'),num(s.batA,1)+' A'],[T('soh'),num(s.soh)+' %']]);
  $('tLim').innerHTML=rows([[T('lim'),s.wMaxLimEna?T('limOn'):T('limOff')],[T('limVal'),num((Number(s.wMaxLimPct)||0)/100,1)+' %']]);
  $('nLim').textContent=s.lastWriteNote||T('noWrite');
  if(s.tasmotaOk){
    $('tTas').innerHTML=rows([[T('power'),num(s.tasmotaPowerW)+' W'],[T('eimp'),num(s.tasmotaTotalKwh,2)+' kWh'],[T('eexp'),num(s.tasmotaExportKwh,2)+' kWh']]);
    $('nTas').textContent=s.tasmotaSensorKey||'';
  }else{$('tTas').innerHTML='';$('nTas').textContent=T('tasOff');}
  $('age').textContent=T('age')(Math.floor((s.lastHitAgeMs||0)/1000));
}
refresh();setInterval(()=>{if(!document.hidden)refresh();},2000);
</script></body></html>
)HTML";

const char HISTORY_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Verlauf · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>
.main:has(.hist){height:100vh;overflow:auto;padding-bottom:16px}
.hist{display:flex;flex-direction:column;gap:12px;flex:1;min-height:0}
.hist .panel{display:flex;flex-direction:column;min-height:0;flex:1 1 0;padding:16px 20px 12px}
.hist .panel h2{margin-bottom:8px}
.hist .hint{margin-top:4px}
.chart{display:block;width:100%;flex:1 1 0;height:0;min-height:140px;overflow:visible}
@media(max-height:720px),(max-width:860px){.main:has(.hist){height:auto;overflow:visible}.chart{flex:none;height:260px}}
.chart .ax{stroke:var(--rule);stroke-width:1}
.chart .zero{stroke:var(--rule2);stroke-width:1}
.chart text{fill:var(--dim);font:400 12px var(--font)}
.chart .ln{fill:none;stroke-width:2;stroke-linejoin:round;stroke-linecap:round}
.chart .cur{stroke:var(--dim);stroke-width:1;stroke-dasharray:3 3}
.legend{display:flex;gap:8px;flex-wrap:wrap;margin:0 0 10px}
.legend .chip{cursor:pointer;user-select:none}
.legend .chip.off{opacity:.4}
.legend .chip i{width:10px;height:10px;border-radius:3px;display:inline-block}
:root{--c-pv:#f59e3b;--c-house:#ef6f62;--c-imp:#b39ddb;--c-exp:#5fd38d;--c-chg:#f1c453;--c-dis:#5aa9f0}
.tabs{display:flex;gap:6px;flex-wrap:wrap;margin:0 0 12px}
.tabs button{padding:6px 14px;font-size:14px}
.tabs button:not(.on){background:var(--plate2);color:var(--ink)}
.read{font-size:13px;color:var(--dim);min-height:20px;margin-top:6px}
.kpis{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:12px;margin-top:14px}
.kpis div{background:var(--well);border-radius:12px;padding:10px 12px}
.kpis b{display:block;font-size:20px;font-weight:600;color:var(--ink)}
.kpis span{font-size:12px;color:var(--dim)}
@media(max-width:860px){.kpis{grid-template-columns:repeat(2,1fr)}}
</style>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/><path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg><div><b>HIT-Proxy</b><span>SunSpec für Consolinno</span></div></div>
  <nav>
    <a href="/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l2-6 4 12 2-6h6"/></svg> Live</a>
    <a class="on" aria-current="page" href="/history"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 19V5M4 19h16M8 15l3-4 3 2 4-6"/></svg> Verlauf</a>
    <div class="nav-grp">Datenquellen</div>
    <a href="/config#hit"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/></svg> HIT-Wechselrichter</a>
    <a href="/config#tasmota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/></svg> Tasmota-Zähler</a>
    <a href="/config#cloud"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/></svg> Hoymiles-Cloud</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#netz"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/></svg> Netzwerk und Zugang</a>
    <a href="/config#display"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/></svg> Display</a>
    <a href="/config#ctrl"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></svg> Akku-Steuerung</a>
    <a href="/debug"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/></svg> Protokoll</a>
    <a href="/ota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/></svg> Firmware und Reset</a>
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
        <option value="light">Hell</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <h1 data-t="title">Verlauf</h1>
    <p class="sub" data-t="sub">Leistung der letzten 24 Stunden und Energie je Tag, Monat und Jahr.</p>
  </div>
</div>

<div class="hist">
<section class="panel day">
  <h2 data-t="h24">Letzte 24 Stunden</h2>
  <div class="tabs" id="mode24"></div>
  <div class="legend" id="leg"></div>
  <svg id="c24" class="chart" viewBox="0 0 960 300" role="img" aria-label="Leistung der letzten 24 Stunden"></svg>
  <div class="read" id="r24"></div>
  <p class="hint" data-t="bal">Flächen über der Null-Linie: PV, Netzbezug und Akku entladen. Die Linien zeigen, wohin die Energie geht: rot das Haus, gelb Akku laden. Unter der Null-Linie nur die Einspeisung.</p>
</section>

<section class="panel bars">
  <h2 data-t="hE">Energie</h2>
  <div class="tabs" id="tabs"></div>
  <div class="legend" id="legB"></div>
  <svg id="cb" class="chart" viewBox="0 0 960 280" role="img" aria-label="Energie je Zeitraum"></svg>
  <div class="kpis" id="kpis"></div>
  <p class="hint" id="since"></p>
</section>
</div>
</main></div>

<script>
const I18N={de:{},en:{}};
let LANG='de';
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
const TX={
 de:{title:'Verlauf',sub:'Leistung der letzten 24 Stunden und Energie je Tag, Monat und Jahr.',h24:'Letzte 24 Stunden',hE:'Energie',
   pv:'PV',house:'Haus',use:'Haus',grid:'Netz',bat:'Akku',days:'Tage',months:'Monate',years:'Jahre',total:'Gesamt',
   gen:'Erzeugt',use:'Verbraucht',imp:'Bezug',exp:'Einspeisung',chg:'Akku laden',dis:'Akku entladen',
   aut:'Autarkie',self:'Eigenverbrauch',none:'Noch keine Daten. Der Verlauf beginnt, sobald der ESP Messwerte und die Uhrzeit hat.',
   since:d=>'Aufzeichnung seit '+d+'. Erzeugung, Bezug und Einspeisung aus den HIT-Zählern, Verbrauch und Akku vom ESP aufsummiert.',
   cur:'Laufender Zeitraum',
   bal:'Flächen über der Null-Linie: PV, Netzbezug und Akku entladen. Die Linien zeigen, wohin die Energie geht: rot das Haus, gelb Akku laden. Unter der Null-Linie nur die Einspeisung.',
   pos:'Flächen: PV, Netzbezug und Akku entladen. Linien: rot das Haus, gelb Akku laden, grün die Einspeisung.',
   mBal:'Bilanz',mPos:'Alles positiv'},
 en:{title:'History',sub:'Power over the last 24 hours and energy per day, month and year.',h24:'Last 24 hours',hE:'Energy',
   pv:'Solar',house:'Home',use:'Home',grid:'Grid',bat:'Battery',days:'Days',months:'Months',years:'Years',total:'Total',
   gen:'Produced',use:'Consumed',imp:'Import',exp:'Export',chg:'Battery charge',dis:'Battery discharge',
   aut:'Self-sufficiency',self:'Self-consumption',none:'No data yet. History starts once the ESP has readings and the time.',
   since:d=>'Recording since '+d+'. Production, import and export from the HIT counters; consumption and battery summed by the ESP.',
   cur:'Current period',
   bal:'Areas above zero: solar, grid import and battery discharge. The lines show where the energy goes: red the home, yellow battery charging. Only export is below zero.',
   pos:'Areas: solar, grid import and battery discharge. Lines: red the home, yellow battery charging, green export.',
   mBal:'Balance',mPos:'All positive'}
};
const T=k=>(TX[LANG]||TX.de)[k];
const $=id=>document.getElementById(id);
const nf=(n,d=0)=>(Number(n)||0).toLocaleString(LANG==='en'?'en-US':'de-DE',{minimumFractionDigits:d,maximumFractionDigits:d});
const SER=[['pv','var(--c-pv)'],['use','var(--c-house)'],['imp','var(--c-imp)'],['exp','var(--c-exp)'],['chg','var(--c-chg)'],['dis','var(--c-dis)']];
const hidden=new Set();
let D24=null;
let MODE=localStorage.getItem('histMode')==='pos'?'pos':'bal';
const S=k=>MODE==='pos'?D24[k].map(v=>v==null?null:Math.abs(v)):D24[k];
function modeTabs(){
  $('mode24').innerHTML=[['bal','mBal'],['pos','mPos']].map(([m,l])=>`<button type="button" class="${MODE===m?'on':''}" data-m="${m}">${T(l)}</button>`).join('');
  $('mode24').querySelectorAll('button').forEach(b=>b.onclick=()=>{MODE=b.dataset.m;localStorage.setItem('histMode',MODE);modeTabs();draw24();});
  const h=document.querySelector('[data-t="bal"]');if(h)h.textContent=T(MODE);
}
const X0=56;let Y0=16,Y1=264,X1=944,W24=960,WB=960,H24=300,HB=280;
const fit=(el,prefer)=>{
  const r=el.getBoundingClientRect();
  const w=Math.max(320,Math.round(r.width||960));
  let h=Math.round(r.height||prefer);
  if(h<120)h=prefer;
  el.setAttribute('viewBox',`0 0 ${w} ${h}`);
  return {w,h};
};

function niceStep(range){const raw=range/5,p=Math.pow(10,Math.floor(Math.log10(raw||1))),m=raw/p;return (m<1.5?1:m<3?2:m<7?5:10)*p;}
function legend(){
  $('leg').innerHTML=SER.map(([k,c])=>`<span class="chip ${hidden.has(k)?'off':'on'}" data-k="${k}"><i style="background:${c}"></i>${T(k)}</span>`).join('');
  $('leg').querySelectorAll('.chip').forEach(e=>e.onclick=()=>{const k=e.dataset.k;hidden.has(k)?hidden.delete(k):hidden.add(k);legend();draw24();});
}
function draw24(){
  const svg=$('c24');
  if(!D24||!D24.valid){const fh=fit(svg,260);svg.innerHTML=`<text x="50%" y="${Math.round(fh.h/2)}" text-anchor="middle">${T('none')}</text>`;return;}
  ({w:W24,h:H24}=fit(svg,300));X1=W24-16;Y0=16;Y1=H24-36;
  const n=D24.pv.length;let lo=0,hi=0;
  SER.forEach(([k])=>{if(hidden.has(k))return;S(k).forEach(v=>{if(v!=null){lo=Math.min(lo,v);hi=Math.max(hi,v);}});});
  if(hi-lo<100)hi=lo+100;
  const step=niceStep(hi-lo);lo=Math.floor(lo/step)*step;hi=Math.ceil(hi/step)*step;
  const x=i=>X0+(X1-X0)*i/(n-1),y=v=>Y1-(Y1-Y0)*(v-lo)/(hi-lo);
  let g='';
  for(let v=lo;v<=hi+1e-6;v+=step){g+=`<line class="ax" x1="${X0}" x2="${X1}" y1="${y(v)}" y2="${y(v)}"/><text x="${X0-8}" y="${y(v)+4}" text-anchor="end">${nf(v/1000,Math.abs(step)<1000?1:0)} kW</text>`;}
  g+=`<line class="zero" x1="${X0}" x2="${X1}" y1="${y(0)}" y2="${y(0)}"/>`;
  for(let i=0;i<n;i++){const ts=new Date((D24.end-(n-1-i)*D24.step)*1000);
    if(ts.getMinutes()<5&&ts.getHours()%3===0)g+=`<text x="${x(i)}" y="${Y1+20}" text-anchor="middle">${String(ts.getHours()).padStart(2,'0')}:00</text>`;}
  const sum=k=>D24[k].reduce((a,v)=>a+Math.abs(v||0),0);
  const order=SER.filter(([k])=>!hidden.has(k)).sort((a,b)=>sum(b[0])-sum(a[0]));
  const segs=k=>{const out=[];let cur=null;S(k).forEach((v,i)=>{if(v==null){cur=null;return;}if(!cur){cur=[];out.push(cur);}cur.push([x(i),y(v)]);});return out;};
  const y0=y(0).toFixed(1);
  // Verbraucher (Haus, Akku laden) liegen über den Quellen, die sie decken: nur als Linie, sonst verdecken ihre Flächen sie.
  // Ohne Vorzeichen liegt auch die Einspeisung in der PV-Fläche und wird deshalb ebenfalls zur Linie.
  const LINE=new Set(MODE==='pos'?['use','chg','exp']:['use','chg']);
  order.forEach(([k,c])=>{if(LINE.has(k))return;segs(k).forEach(p=>{if(p.length<2)return;
    g+=`<path d="M${p[0][0].toFixed(1)} ${y0}`+p.map(q=>`L${q[0].toFixed(1)} ${q[1].toFixed(1)}`).join('')+`L${p[p.length-1][0].toFixed(1)} ${y0}Z" style="fill:${c};opacity:.28"/>`;});});
  [...order.filter(([k])=>!LINE.has(k)),...order.filter(([k])=>LINE.has(k))].forEach(([k,c])=>{segs(k).forEach(p=>{
    g+=`<path class="ln" d="`+p.map((q,i)=>(i?'L':'M')+q[0].toFixed(1)+' '+q[1].toFixed(1)).join('')+`" style="stroke:${c}${LINE.has(k)?';stroke-width:2.5':''}"/>`;});});
  g+=`<line id="cur" class="cur" x1="0" x2="0" y1="${Y0}" y2="${Y1}" style="display:none"/>`;
  svg.innerHTML=g;
  svg.onmousemove=e=>{const r=svg.getBoundingClientRect(),px=(e.clientX-r.left)*W24/r.width;
    const i=Math.round((px-X0)/(X1-X0)*(n-1));if(i<0||i>=n)return;
    const c=$('cur');c.setAttribute('x1',x(i));c.setAttribute('x2',x(i));c.style.display='';
    const ts=new Date((D24.end-(n-1-i)*D24.step)*1000);
    $('r24').innerHTML=ts.toLocaleTimeString(LANG==='en'?'en-GB':'de-DE',{hour:'2-digit',minute:'2-digit'})+' · '+
      SER.map(([k,col])=>`<span style="color:${col}">${T(k)} ${D24[k][i]==null?'–':nf(Math.abs(D24[k][i]))+' W'}</span>`).join(' · ');};
  svg.onmouseleave=()=>{$('cur').style.display='none';$('r24').textContent='';};
}

const BSER=[['pv','var(--c-pv)','gen'],['house','var(--c-house)','use'],['imp','var(--c-imp)','imp'],['exp','var(--c-exp)','exp'],['chg','var(--c-chg)','chg'],['dis','var(--c-dis)','dis']];
const hiddenB=new Set();
let UNIT='day',BARS=null;
function tabs(){
  const u=[['day','days'],['month','months'],['year','years'],['total','total']];
  $('tabs').innerHTML=u.map(([k,l])=>`<button type="button" class="${UNIT===k?'on':''}" data-u="${k}">${T(l)}</button>`).join('');
  $('tabs').querySelectorAll('button').forEach(b=>b.onclick=()=>{UNIT=b.dataset.u;tabs();loadBars();});
  legendB();
}
function legendB(){
  $('legB').innerHTML=BSER.map(([k,c,l])=>`<span class="chip ${hiddenB.has(k)?'off':'on'}" data-k="${k}"><i style="background:${c}"></i>${T(l)}</span>`).join('');
  $('legB').querySelectorAll('.chip').forEach(e=>e.onclick=()=>{const k=e.dataset.k;hiddenB.has(k)?hiddenB.delete(k):hiddenB.add(k);legendB();if(BARS)drawBars();});
}
function label(k){
  if(UNIT==='day'){const p=k.split('-');return p[2]+'.'+p[1]+'.';}
  if(UNIT==='month'){const p=k.split('-');return new Date(+p[0],+p[1]-1,1).toLocaleDateString(LANG==='en'?'en-GB':'de-DE',{month:'short',year:'2-digit'});}
  return k;
}
function drawBars(){
  const svg=$('cb');let rows=BARS.bars||[];
  if(UNIT==='total'){const tt=BARS.total||{};rows=[{k:T('total'),pv:+tt.pv,imp:+tt.imp,exp:+tt.exp,house:null,chg:null,dis:null}];}
  if(!rows.length){const fh=fit(svg,240);svg.innerHTML=`<text x="50%" y="${Math.round(fh.h/2)}" text-anchor="middle">${T('none')}</text>`;$('kpis').innerHTML='';return;}
  const vis=BSER.filter(([k])=>!hiddenB.has(k));
  let hi=0;rows.forEach(r=>vis.forEach(([k])=>{if(r[k]!=null)hi=Math.max(hi,+r[k]);}));if(hi<=0)hi=1;
  const step=niceStep(hi);hi=Math.ceil(hi/step)*step;
  ({w:WB,h:HB}=fit(svg,280));
  const y0=20,y1=HB-36,x0=56,x1=WB-16,slot=(x1-x0)/rows.length,bw=Math.max(2,Math.min(22,slot/(vis.length+1)));
  const y=v=>y1-(y1-y0)*v/hi;let g='';
  for(let v=0;v<=hi+1e-6;v+=step)g+=`<line class="ax" x1="${x0}" x2="${x1}" y1="${y(v)}" y2="${y(v)}"/><text x="${x0-8}" y="${y(v)+4}" text-anchor="end">${nf(v,step<1?1:0)}</text>`;
  const every=Math.ceil(rows.length/Math.max(6,Math.floor((x1-x0)/70)));
  rows.forEach((r,i)=>{const cx=x0+slot*i+slot/2;
    vis.forEach(([k,c,l],j)=>{if(r[k]==null)return;const v=+r[k],bx=cx+(j-vis.length/2)*bw;
      g+=`<rect x="${bx.toFixed(1)}" y="${y(v).toFixed(1)}" width="${(bw-1).toFixed(1)}" height="${(y1-y(v)).toFixed(1)}" rx="2" style="fill:${c}"><title>${label(r.k)} · ${T(l)} ${nf(v,1)} kWh</title></rect>`;});
    if(i%every===0)g+=`<text x="${cx}" y="${y1+18}" text-anchor="middle">${UNIT==='total'?r.k:label(r.k)}</text>`;});
  g+=`<text x="${x0}" y="12">kWh</text>`;
  svg.innerHTML=g;
  const cur=rows[rows.length-1],pv=+cur.pv||0,imp=+cur.imp||0,exp=+cur.exp||0,use=cur.house==null?null:+cur.house;
  const k=(lbl,v)=>`<div><span>${lbl}</span><b>${v}</b></div>`;
  $('kpis').innerHTML=k(T('gen'),nf(pv,1)+' kWh')+k(T('use'),use==null?'–':nf(use,1)+' kWh')+k(T('imp'),nf(imp,1)+' kWh')+
    k(T('exp'),nf(exp,1)+' kWh')+k(T('aut'),use?nf(Math.max(0,Math.min(100,100*(1-imp/use))))+' %':'–')+
    k(T('self'),pv?nf(Math.max(0,Math.min(100,100*(1-exp/pv))))+' %':'–');
  const s=String(BARS.since||'');$('since').textContent=s.length===8?T('since')(s.slice(6)+'.'+s.slice(4,6)+'.'+s.slice(0,4)):'';
}
async function loadBars(){
  const u=UNIT==='total'?'day':UNIT,n=UNIT==='day'?31:UNIT==='month'?24:40;
  BARS=await(await fetch(`/api/history/bars?unit=${u}&n=${n}`)).json();drawBars();
}
async function load24(){
  D24=await(await fetch('/api/history/day')).json();
  if(D24.valid){const m=(a,f)=>a.map(v=>v==null?null:f(v));
    D24.imp=m(D24.grid,v=>Math.max(0,v));D24.exp=m(D24.grid,v=>Math.min(0,v));
    D24.dis=m(D24.bat,v=>Math.max(0,v));D24.chg=m(D24.bat,v=>Math.max(0,-v));
    D24.use=m(D24.house,v=>Math.abs(v));}
  draw24();
}
async function boot(){
  const s=await(await fetch('/api/status')).json();applyThemeLang(s);
  document.querySelectorAll('[data-t]').forEach(e=>{const v=T(e.dataset.t);if(typeof v==='string')e.textContent=v;});
  modeTabs();legend();tabs();await load24();await loadBars();
}
boot();
setInterval(()=>{if(!document.hidden){load24();if(UNIT==='day')loadBars();}},60000);
let rsz=0;const redraw=()=>{clearTimeout(rsz);rsz=setTimeout(()=>{draw24();if(BARS)drawBars();},120);};
if(window.ResizeObserver){const ro=new ResizeObserver(redraw);ro.observe($('c24'));ro.observe($('cb'));}
else window.addEventListener('resize',redraw);
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
  <div class="logo"><svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/><path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg><div><b>HIT-Proxy</b><span>SunSpec für Consolinno</span></div></div>
  <nav>
    <a href="/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l2-6 4 12 2-6h6"/></svg> Live</a>
    <a href="/history"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 19V5M4 19h16M8 15l3-4 3 2 4-6"/></svg> Verlauf</a>
    <div class="nav-grp">Datenquellen</div>
    <a href="/config#hit"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/></svg> HIT-Wechselrichter</a>
    <a href="/config#tasmota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/></svg> Tasmota-Zähler</a>
    <a href="/config#cloud"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/></svg> Hoymiles-Cloud</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#netz"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/></svg> Netzwerk und Zugang</a>
    <a href="/config#display"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/></svg> Display</a>
    <a href="/config#ctrl"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></svg> Akku-Steuerung</a>
    <a href="/debug"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/></svg> Protokoll</a>
    <a href="/ota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/></svg> Firmware und Reset</a>
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
        <option value="light">Hell</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<div class="page-head">
  <div>
    <h1 id="cfgTitle">Einstellungen</h1>
    <p class="sub" id="cfgSub"></p>
  </div>
  <div class="chips" id="chips"></div>
</div>
<form id="f" class="cfg">
<div class="card" id="hit" data-sub="Pflicht. Von hier kommen alle Messwerte."><strong>HIT-Wechselrichter</strong>
<label>Verbindung</label>
<select name="hitMode"><option value="0">TCP (Modbus TCP)</option><option value="1">Kabel RS485 (RTU)</option></select>
<label>HIT IP / Host</label><input name="hitHost" placeholder="192.0.2.50"/>
<label>HIT Port</label><input name="hitPort" type="number"/>
<label>Unit-ID</label><input name="hitUnitId" type="number"/>
<label>Poll (ms)</label><input name="hitPollMs" type="number"/>
<label>RS485-Baudrate (nur bei Kabel)</label><input name="hitRtuBaud" type="number"/>
<p class="hint">TCP: IP des HIT, Port 502. Der HIT erlaubt nur eine Verbindung; solange ein anderes Programm mit ihm verbunden ist, bekommt der ESP keine Antwort.</p>
<p class="hint">RS485: Anschluss am HIT meist PARA2, RJ45 Pin 8 = A, Pin 7 = B, 8N1, Unit-ID 1. Am HIT eingestellte Baudrate übernehmen (Werk oft 115200, häufig auf 9600 gestellt).</p>
<label><input name="demoMode" type="checkbox"/> <b>Demo-Modus</b> (Fake-Daten, kein HIT-Schreiben)</label>
<p class="hint">Für Board-/Web-/Display-Test ohne Hoymiles. SunSpec :1502 lokal ansprechbar; Consolinno nicht nötig.</p>
<button class="sec" type="button" onclick="poll()" style="margin-top:16px">HIT jetzt abfragen</button>
</div>
<div class="card" id="sunspec" data-sub="Consolinno liest den Proxy als SunSpec-Wechselrichter über Modbus TCP."><strong>Consolinno</strong>
<label>Modbus-Port</label><input name="sunspecPort" type="number"/>
<label>Unit-ID</label><input name="sunspecUnitId" type="number"/>
<label><input name="invertPowerSign" type="checkbox"/> Vorzeichen der Leistung umkehren</label>
<p class="hint">In Consolinno als SunSpec-Gerät eintragen: IP vom Display, Port 1502, Unit-ID 1. Die Registerkarte liegt ab Adresse 0, 40000 und 50000.</p>
</div>
<div class="card" id="netz" data-sub="Verbindung ins Heimnetz und Anmeldung an dieser Oberfläche."><strong>Netzwerk und Zugang</strong>
<label>Gerätename</label><input name="deviceName"/>
<label><input name="useEthernet" type="checkbox"/> Ethernet verwenden</label>
<label><input name="wifiEnabled" type="checkbox"/> WLAN verwenden</label>
<label>Heim-WLAN: Name (SSID)</label><input name="staSsid" autocomplete="off" placeholder="leer = kein Heim-WLAN"/>
<label>Heim-WLAN: Passwort</label><input name="staPass" type="password" autocomplete="new-password" placeholder="leer lassen = unverändert"/>
<p class="hint">Mit Heim-WLAN ist der ESP auch ohne Kabel im Netz und hat Internet, zum Beispiel für die Hoymiles-Cloud.
Die neue Adresse steht danach auf dem Display (Seite System).</p>
<label><input name="wifiApMode" type="checkbox"/> Geräte-WLAN zusätzlich anlassen (Notzugang)</label>
<label>Name des Geräte-WLANs</label><input name="wifiSsid"/>
<label>Zugangscode (Passwort des Geräte-WLANs, steht auf dem Display)</label><input name="wifiPass" readonly/>
<p class="hint">Ohne Heim-WLAN und ohne Kabel bleibt das Geräte-WLAN immer an, damit das Gerät erreichbar bleibt.</p>
<button class="sec" type="button" onclick="newCode()" style="margin-top:12px">Neuen Zugangscode erzeugen</button>
<p class="hint">Der Code bleibt über Neustarts und Updates erhalten. Ein neuer Code gilt sofort für die Anmeldung und das Geräte-WLAN
und steht dann auf dem Display; wer mit dem alten Code angemeldet ist, muss sich neu anmelden. Das eigene Web-Passwort bleibt gültig.</p>
<label>Benutzername</label><input name="webUser"/>
<label>Eigenes Web-Passwort</label><input name="webPass" type="password" placeholder="leer lassen = nur Zugangscode"/>
<p class="hint">Anmelden mit dem Benutzernamen und entweder dem eigenen Passwort oder dem Zugangscode vom Display.</p>
<label><input name="httpsEnabled" type="checkbox"/> HTTPS (Port 443)</label>
<label><input name="httpsRedirect" type="checkbox"/> Aufrufe über HTTP auf HTTPS umleiten</label>
<p class="hint">Der ESP erzeugt sein Zertifikat selbst. Der Browser warnt deshalb beim ersten Aufruf; vergleiche den Fingerabdruck
unten mit dem im Browser und bestätige dann. Während der Einrichtung und falls HTTPS nicht startet, bleibt HTTP erreichbar.</p>
<div id="tlsBox">
<label>HTTPS-Zertifikat</label>
<table><tbody id="tlsInfo"><tr><td class="muted">Lade…</td></tr></tbody></table>
<label>Eigenes Zertifikat (PEM, Zwischenzertifikate dahinter)</label>
<textarea id="tlsCrt" rows="5" spellcheck="false" placeholder="-----BEGIN CERTIFICATE-----"></textarea>
<input type="file" id="tlsCrtFile" accept=".pem,.crt,.cer,.txt" onchange="tlsFile(this,'tlsCrt')"/>
<label>Privater Schlüssel (PEM, ohne Passwort)</label>
<textarea id="tlsKey" rows="4" spellcheck="false" placeholder="-----BEGIN PRIVATE KEY-----"></textarea>
<input type="file" id="tlsKeyFile" accept=".pem,.key,.txt" onchange="tlsFile(this,'tlsKey')"/>
<div class="row">
<button class="sec" type="button" onclick="tlsUpload()">Eigenes Zertifikat übernehmen</button>
<button class="sec" type="button" onclick="tlsSelf()">Selbst signiertes neu erzeugen</button>
<span id="tlsMsg" class="hint" style="margin:0"></span>
</div>
<p class="hint">Ein eigenes Zertifikat, etwa von der Hausnetz-CA oder Let's Encrypt über DNS-Prüfung, erspart die Warnung. Der ESP prüft,
ob Schlüssel und Zertifikat zusammenpassen, und startet HTTPS danach neu. „Selbst signiertes neu erzeugen“ löscht das eigene Zertifikat
und nimmt die aktuelle IP-Adresse ins neue auf.</p>
</div>
</div>
<div class="card" id="display" data-sub="Anzeige am Gerät. Antippen wechselt die Seite."><strong>Display</strong>
<label>Live-Ansicht</label>
<canvas id="scr" width="480" height="222" title="Klicken = nächste Seite"
 style="display:block;width:100%;max-width:480px;aspect-ratio:480/222;border-radius:14px;background:#000;cursor:pointer"></canvas>
<p class="hint">Zeigt das Bild des Displays, alle 2 Sekunden neu, auch wenn das Licht am Gerät aus ist. Ein Klick wirkt wie Antippen.</p>
<label><input name="displayEnabled" type="checkbox"/> Display eingeschaltet</label>
<label>Helligkeit (0 bis 255)</label><input name="displayBrightness" type="number" min="0" max="255"/>
<label><input name="displayAutoPage" type="checkbox"/> Seiten automatisch wechseln</label>
<label>Seitenwechsel alle (Sekunden)</label><input name="displayPageS" type="number" min="3" max="600"/>
<p class="hint">Aus: Die Live-Seite bleibt stehen. Antippen blättert, nach 30 Sekunden ohne Berührung geht es zurück zur Live-Seite.</p>
<label>Ausschalten nach (Sekunden ohne Berührung, 0 = immer an)</label><input name="displayTimeoutS" type="number" min="0" max="36000"/>
<p class="hint">Antippen schaltet das Display wieder ein. Bei einer Störung (HIT antwortet nicht) geht es von selbst an.</p>
<label><input name="setupComplete" type="checkbox"/> Einrichtung abgeschlossen (QR-Code-Seite ausblenden)</label>
<p class="hint">Seiten: Live, Strings und Netz, System.</p>
</div>
<div class="card" id="ctrl" data-sub="Schreibt Grenzwerte an den HIT. Standardmäßig aus."><strong>Akku-Steuerung</strong>
<label><input name="hitWriteEnabled" type="checkbox"/> Schreiben an HIT erlauben</label>
<label><input name="sunspecWriteToHit" type="checkbox"/> Consolinno Model-123 → HIT weiterleiten</label>
<label>Max. Ladeleistung in % (Reg. 306)</label><input name="batMaxChargePct" type="number" min="10" max="100"/>
<label>Max. Entladeleistung in % (Reg. 307)</label><input name="batMaxDischargePct" type="number" min="10" max="100"/>
<label>Max. Ladestand in % (Reg. 308)</label><input name="batMaxSoc" type="number" min="10" max="100"/>
<label>Min. Ladestand in % (Reg. 309)</label><input name="batMinSoc" type="number" min="10" max="90"/>
<label><input name="apcEnabled" type="checkbox"/> Wirkleistungsbegrenzung APC (Reg. 279)</label>
<label>APC-Grenze in 0,1 % (Reg. 280, 1000 = 100 %)</label><input name="apcPwrLmtPct10" type="number" min="0" max="1000"/>
<p class="hint">Alles standardmäßig aus. 279/280 liegen im Netzschutz-Bereich 101 bis 299: Der ESP schreibt dort nur mit FC16, weil ein FC6-Befehl laut Hoymiles alle Netzschutz-Werte auf Werkseinstellung setzt. Eine Consolinno-Begrenzung (Model 123) geht nur auf APC, nicht auf den Akku.</p>
<button class="sec" type="button" onclick="applyCtrl()">Jetzt an HIT schreiben</button>
</div>
<div class="card" id="tasmota" data-sub="Optionaler Netzzähler mit Tasmota-Lesekopf."><strong>Tasmota-Zähler</strong>
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
<div class="card" id="mqtt" data-sub="Messwerte per MQTT an Home Assistant, optional auch als Datenquelle."><strong>Home Assistant</strong>
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
<div class="card" id="cloud" data-sub="Ersatzquelle, wenn HIT und MQTT-Eingang ausfallen. Nur lesen."><strong>Hoymiles-Cloud</strong>
<label><input name="cloudEnabled" type="checkbox"/> aktiv (wenn HIT &amp; MQTT-in tot)</label>
<label>API Host</label><input name="cloudHost" placeholder="neapi.hoymiles.com"/>
<label>Cloud User / E-Mail</label><input name="cloudUser"/>
<label>Passwort</label><input name="cloudPass" type="password"/>
<label>Station / Plant ID (sid)</label><input name="cloudStationId" placeholder="z.B. 1234567"/>
<label>Poll (ms)</label><input name="cloudPollMs" type="number"/>
<label>Max Alter (ms)</label><input name="cloudMaxAgeMs" type="number"/>
<p class="hint">Inoffizielle S-Miles API — nur Lesen. Schreiben (§14a) nur über HIT Modbus.</p>
</div>
<div class="card" id="gateway" data-sub="Andere Programme lesen den HIT über den ESP, statt selbst eine Verbindung aufzubauen."><strong>Modbus-Gateway</strong>
<label><input name="gatewayEnabled" type="checkbox"/> Gateway einschalten</label>
<label>Port</label><input name="gatewayPort" type="number" min="1" max="65535"/>
<p class="hint">Der HIT erlaubt nur eine Verbindung, und die hält der ESP. Home Assistant oder andere Modbus-Programme
tragen statt des HIT die IP des ESP mit diesem Port ein; Register und Unit-ID bleiben gleich.</p>
<p class="hint">Lesen geht immer. Schreiben reicht der ESP nur durch, wenn unter Akku-Steuerung „Schreiben an HIT erlauben“ aktiv ist,
sonst bekommt der Client eine Modbus-Fehlermeldung. Jeder Schreibzugriff steht im Protokoll.</p>
</div>
<div class="card" id="knx" data-sub="Messwerte als Gruppentelegramme über KNXnet/IP Routing."><strong>KNX</strong>
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
<button class="sec" type="button" onclick="load()">Änderungen verwerfen</button>
<span class="meta" id="saved"></span>
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
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
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
function route(){
  const cards=[...document.querySelectorAll('#f > .card')];
  const id=location.hash.slice(1);
  const cur=cards.find(c=>c.id===id)||cards[0];
  cards.forEach(c=>c.classList.toggle('show',c===cur));
  document.getElementById('cfgTitle').textContent=cur.querySelector('strong').textContent;
  document.getElementById('cfgSub').textContent=cur.dataset.sub||'';
  document.title=cur.querySelector('strong').textContent+' · HIT Proxy';
  document.querySelectorAll('.side nav a').forEach(a=>{const on=a.getAttribute('href')==='/config#'+cur.id;
    a.classList.toggle('on',on);if(on)a.setAttribute('aria-current','page');else a.removeAttribute('aria-current');});
  window.scrollTo(0,0);
}
window.addEventListener('hashchange',route);
async function drawScreen(){
  const card=document.getElementById('display');
  if(!card||!card.classList.contains('show')||document.hidden)return;
  try{
    const r=await fetch('/api/screen',{cache:'no-store'});if(!r.ok)return;
    const w=Number(r.headers.get('X-Width'))||480,h=Number(r.headers.get('X-Height'))||222;
    const b=new Uint8Array(await r.arrayBuffer());
    const cv=document.getElementById('scr');if(cv.width!==w){cv.width=w;cv.height=h;}
    const ctx=cv.getContext('2d'),img=ctx.createImageData(w,h),d=img.data;
    for(let i=0,j=0;i+1<b.length;i+=2,j+=4){const v=(b[i]<<8)|b[i+1];
      d[j]=((v>>11)&31)*255/31;d[j+1]=((v>>5)&63)*255/63;d[j+2]=(v&31)*255/31;d[j+3]=255;}
    ctx.putImageData(img,0,0);
  }catch(e){}
}
setInterval(drawScreen,2000);
window.addEventListener('hashchange',drawScreen);
document.getElementById('scr').addEventListener('click',async()=>{
  await fetch('/api/display/tap',{method:'POST'});setTimeout(drawScreen,400);});
async function load(){
  const [c,meta]=await Promise.all([fetch('/api/config').then(r=>r.json()),fetch('/api/knx/meta').then(r=>r.json())]);
  const f=document.getElementById('f');
  for(const[k,v] of Object.entries(c)){if(k==='knxCh')continue;const el=f.elements.namedItem(k);if(!el)continue;if(el.type==='checkbox')el.checked=!!v;else el.value=v;}
  renderKnx(meta.points||[], c.knxCh||[]); refresh(); route(); loadTls();
}
async function loadTls(){
  const t=await fetch('/api/tls').then(r=>r.json()).catch(()=>null);const tb=document.getElementById('tlsInfo');if(!tb)return;
  if(!t){tb.innerHTML='<tr><td>Nicht abrufbar</td></tr>';return;}
  const esc=s=>String(s||'–').replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
  const row=(k,v)=>`<tr><td>${k}</td><td>${v}</td></tr>`;
  tb.innerHTML=row('Status',!t.enabled?'aus':t.running?'<span class="ok">läuft</span>':'<span class="bad">nicht gestartet</span>')+
    row('Zertifikat',t.source==='own'?'eigenes'+(t.chain>1?' (mit '+(t.chain-1)+' Zwischenzertifikat'+(t.chain>2?'en':'')+')':''):'selbst signiert')+
    row('Ausgestellt für',esc(t.subject))+(t.source==='own'?row('Aussteller',esc(t.issuer)):'')+
    row('Gültig bis',esc(t.validTo))+row('SHA-256',`<code style="font-size:12px">${esc(t.sha256)}</code>`);
}
function tlsFile(inp,id){const f=inp.files&&inp.files[0];if(!f)return;const r=new FileReader();
  r.onload=()=>{document.getElementById(id).value=r.result;};r.readAsText(f);}
function tlsMsg(t,ok){const m=document.getElementById('tlsMsg');m.textContent=t;m.style.color=ok?'var(--acc)':'var(--fault)';}
async function tlsUpload(){
  const cert=document.getElementById('tlsCrt').value.trim(),key=document.getElementById('tlsKey').value.trim();
  if(!cert||!key){tlsMsg('Zertifikat und Schlüssel einfügen oder Dateien wählen',false);return;}
  const r=await fetch('/api/tls/cert',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({cert,key})}).catch(()=>null);
  const j=r?await r.json().catch(()=>({})):{};
  if(r&&r.ok){document.getElementById('tlsKey').value='';tlsMsg('Übernommen, HTTPS startet neu…',true);setTimeout(()=>location.reload(),4000);}
  else tlsMsg(j.error||'Fehler beim Hochladen',false);
}
async function tlsSelf(){
  if(!confirm('Neues selbst signiertes Zertifikat erzeugen? Ein eigenes Zertifikat wird dabei gelöscht, der Browser warnt danach erneut.'))return;
  const r=await fetch('/api/tls/self',{method:'POST'}).catch(()=>null);
  if(r&&r.ok){tlsMsg('Wird erzeugt, HTTPS startet neu…',true);setTimeout(()=>location.reload(),5000);}else tlsMsg('Fehler',false);
}
async function save(){const f=document.getElementById('f');const body={};
[...f.elements].forEach(el=>{if(!el.name||el.dataset.knx)return;if(el.type==='checkbox')body[el.name]=el.checked;else if(el.type==='number')body[el.name]=Number(el.value);else body[el.name]=el.value;});
body.knxCh=collectKnx();
const m=document.getElementById('saved');
let ok=false;try{const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});ok=r.ok;}catch(e){}
m.textContent=ok?'Gespeichert':'Speichern fehlgeschlagen';m.style.color=ok?'var(--acc)':'var(--fault)';
if(ok){setTimeout(()=>m.textContent='',2500);load();}}
async function poll(){await fetch('/api/poll',{method:'POST'});refresh();}
async function newCode(){
  if(!confirm('Neuen Zugangscode erzeugen? Der alte Code gilt dann nicht mehr. Der neue steht auf dem Display.'))return;
  const r=await fetch('/api/access/new-code',{method:'POST'}).catch(()=>null);
  alert(r&&r.ok?'Neuer Code ist aktiv. Bitte vom Display ablesen; gegebenenfalls neu anmelden.':'Fehler');
  if(r&&r.ok)location.reload();
}
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
  document.getElementById('probeSum').style.color=r.ok?'var(--acc)':'var(--fault)';
  document.getElementById('probeRaw').textContent=r.raw||'';
  if(r.ok){f.tasmotaEnabled.checked=true;refresh();}
}
async function refresh(){const s=await(await fetch('/api/status')).json();
applyThemeLang(s);
const chip=(st,l)=>`<span class="chip ${st}"><span class="dot"></span>${l}</span>`;
document.getElementById('chips').innerHTML=chip(s.hitOk?'on':'off','HIT')+chip(s.sunspecClient?'on':'idle','Consolinno')+
  chip((s.ethOk||s.wifiOk)?'on':'off',s.ipShown||'-')+(s.demoMode?'<span class="tag" style="margin:0;vertical-align:0">Demo</span>':'');}
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
<p class="hint">Den HIT nicht zusätzlich direkt in Consolinno eintragen, sonst sehen beide Wege den HIT gleichzeitig.</p>
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
  const next=r.https?`https://${location.hostname}/`:'/';
  fin.innerHTML=r.https
    ?`<span class="ok">Fertig.</span> Weiter über HTTPS: <a href="${next}">${next}</a><br>`+
     `<span class="muted">Der Browser warnt einmal wegen des eigenen Zertifikats; bestätigen und mit demselben Zugang anmelden.</span>`
    :'<span class="ok">Fertig — weiter zum Live-Dashboard…</span>';
  setTimeout(()=>location.href=next,r.https?2500:1200);
}
boot();
</script></body></html>
)HTML";

const char DEBUG_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="de"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta name="color-scheme" content="dark"/><title>Protokoll · HIT Proxy</title>
<link rel="stylesheet" href="/a.css"/>
<style>.card{margin-bottom:12px}</style>
</head><body><div class="app">
<aside class="side">
  <div class="logo"><svg viewBox="0 0 32 32" aria-hidden="true"><rect width="32" height="32" rx="9" fill="currentColor"/><path d="M18 6 10 18h6l-2 8 8-12h-6z" style="fill:var(--bg)"/></svg><div><b>HIT-Proxy</b><span>SunSpec für Consolinno</span></div></div>
  <nav>
    <a href="/"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 12h4l2-6 4 12 2-6h6"/></svg> Live</a>
    <a href="/history"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 19V5M4 19h16M8 15l3-4 3 2 4-6"/></svg> Verlauf</a>
    <div class="nav-grp">Datenquellen</div>
    <a href="/config#hit"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="1"/><path d="M4 20 20 4M7 8.5h4M13 16c.9-1.1 1.8-1.1 2.7 0s1.8 1.1 2.7 0"/></svg> HIT-Wechselrichter</a>
    <a href="/config#tasmota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 3v5M15 3v5M6 8h12v3a6 6 0 0 1-12 0zM12 17v4"/></svg> Tasmota-Zähler</a>
    <a href="/config#cloud"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M7 18h10a4 4 0 0 0 .4-8A6 6 0 0 0 6 9.6 4.2 4.2 0 0 0 7 18z"/></svg> Hoymiles-Cloud</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#netz"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 9h14v10H5zM8.5 19v-3M12 19v-3M15.5 19v-3M9 9V6h6v3"/></svg> Netzwerk und Zugang</a>
    <a href="/config#display"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="5" width="18" height="12" rx="2.5"/><path d="M9 20h6"/></svg> Display</a>
    <a href="/config#ctrl"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 10.5v3M11 10.5v3"/></svg> Akku-Steuerung</a>
    <a class="on" aria-current="page" href="/debug"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 6h11M9 12h11M9 18h11M4.5 6h.01M4.5 12h.01M4.5 18h.01"/></svg> Protokoll</a>
    <a href="/ota"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 15V4M7.5 8.5 12 4l4.5 4.5M5 14v5h14v-5"/></svg> Firmware und Reset</a>
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
        <option value="light">Hell</option>
      </select>
    </div>
  </div>
</aside>
<main class="main">
<style>
.ltabs{display:flex;gap:6px;margin:0 0 14px}
.ltabs button:not(.on){background:var(--plate2);color:var(--ink)}
.lv{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:8px;background:var(--rule2)}
.lv.W{background:var(--warn)}.lv.E{background:var(--fault)}.lv.I{background:var(--acc)}
#sys td:first-child,#tb td:first-child{white-space:nowrap;color:var(--dim);padding-right:16px}
#sys td:nth-child(2){white-space:nowrap;padding-right:16px}
</style>
<div class="page-head">
  <div>
    <h1>Protokoll</h1>
    <p class="sub" id="psub">Verbindungen, Fehler und Neustarts seit dem letzten Start.</p>
  </div>
</div>
<div class="ltabs">
  <button type="button" id="tSys" class="on" onclick="tab('sys')">System</button>
  <button type="button" id="tW" onclick="tab('w')">§14a-Writes</button>
</div>

<div id="pSys">
<div class="card" id="sysSum">Lade…</div>
<div class="card" style="margin-top:12px">
<div class="row" style="margin-top:0;margin-bottom:12px">
<button class="sec" type="button" onclick="refresh()">Aktualisieren</button>
<button class="sec" type="button" onclick="clrSys()">Protokoll leeren</button>
</div>
<table><thead><tr><th>Zeit</th><th>Quelle</th><th>Meldung</th></tr></thead>
<tbody id="sys"></tbody></table>
</div>
</div>

<div id="pW" class="hide">
<div class="card" id="sum">Lade…</div>
<div class="card" style="margin-top:12px">
<div class="row" style="margin-top:0;margin-bottom:12px">
<button class="sec" type="button" onclick="refresh()">Aktualisieren</button>
<button class="sec" type="button" onclick="clr()">Log leeren</button>
</div>
<table><thead><tr><th>Alter</th><th>FC</th><th>Addr</th><th>Wert</th><th>Hinweis</th></tr></thead>
<tbody id="tb"></tbody></table>
</div>
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
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
function age(ms){const s=Math.floor((Date.now()-window._boot-ms)/1000);if(s<0)return ms+'ms';if(s<60)return s+'s';return Math.floor(s/60)+'m';}
let TAB=location.hash==='#writes'?'w':'sys';
function tab(k){TAB=k;$('tSys').classList.toggle('on',k==='sys');$('tW').classList.toggle('on',k==='w');
  $('pSys').classList.toggle('hide',k!=='sys');$('pW').classList.toggle('hide',k!=='w');
  $('psub').textContent=k==='sys'?'Verbindungen, Fehler und Neustarts seit dem letzten Start.':'SunSpec Model 123: Writes von Consolinno auf Port 1502.';
  history.replaceState(null,'',location.pathname+location.search+(k==='w'?'#writes':''));refresh();}
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
function when(e,boot){
  const d=e.ts?new Date(e.ts*1000):new Date(boot+e.t);
  const today=new Date().toDateString()===d.toDateString();
  return (today?'':d.toLocaleDateString('de-DE',{day:'2-digit',month:'2-digit'})+' ')+d.toLocaleTimeString('de-DE');
}
async function refreshSys(){
  const d=await(await fetch('/api/events')).json();
  const boot=Date.now()-d.uptimeMs;
  const rate=d.hitPolls?(100*d.hitFails/d.hitPolls):0;
  $('sysSum').innerHTML=`<div class="big" style="color:${d.hitFails?(rate>5?'var(--fault)':'var(--warn)'):'var(--acc)'}">HIT: ${d.hitFails} von ${d.hitPolls} Abfragen fehlgeschlagen</div>
    <div class="muted">${d.hitPolls?rate.toFixed(1)+' % seit Start · ':''}läuft seit ${Math.floor(d.uptimeMs/3600000)} h ${Math.floor(d.uptimeMs/60000)%60} min${d.hitErr?' · letzte Ursache: '+esc(d.hitErr):''}</div>`;
  const tb=$('sys');
  tb.innerHTML=(d.events||[]).slice().reverse().map(e=>`<tr><td>${when(e,boot)}</td><td><span class="lv ${e.lv}"></span>${esc(e.src)}</td><td>${esc(e.msg)}</td></tr>`).join('')
    ||'<tr><td colspan="3" class="muted">Noch keine Einträge.</td></tr>';
}
async function clrSys(){await fetch('/api/events/clear',{method:'POST'});refresh();}
async function refresh(){
  const s=await(await fetch('/api/status')).json(); applyThemeLang(s);
  if(TAB==='sys')return refreshSys();
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
tab(TAB);setInterval(()=>{if(!document.hidden)refresh();},3000);
</script></body></html>
)HTML";

bool requireAuth(AsyncWebServerRequest *req) {
  auto &c = ConfigStore::get();
  const char *user = c.webUser[0] ? c.webUser : "admin";
  if (c.displayCode[0] && req->authenticate(user, c.displayCode)) return true;
  if (c.webPass[0] && req->authenticate(user, c.webPass)) return true;
  if (c.wifiPass[0] && req->authenticate(user, c.wifiPass)) return true;

  Serial.printf("Auth fail user=%s\n", user);
  req->requestAuthentication("HIT-Proxy", false); // Default wäre Digest
  return false;
}

// Der Body kommt in mehreren TCP-Stücken; erst das letzte liefert den ganzen Text.
// _tempObject gibt AsyncWebServer beim Ende des Requests selbst frei.
const char *fullBody(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
  constexpr size_t MAX_BODY = 16384;
  if (total > MAX_BODY) {
    if (index == 0) req->send(413, "application/json", "{\"ok\":false,\"error\":\"body too large\"}");
    return nullptr;
  }
  if (index == 0) {
    req->_tempObject = calloc(total + 1, 1);
    if (!req->_tempObject) {
      req->send(500, "application/json", "{\"ok\":false,\"error\":\"no memory\"}");
      return nullptr;
    }
  }
  if (!req->_tempObject || index + len > total) return nullptr;
  memcpy(static_cast<char *>(req->_tempObject) + index, data, len);
  if (index + len < total) return nullptr;
  return static_cast<const char *>(req->_tempObject);
}

void applyJson(JsonObjectConst o, AppConfig &c) {
  if (!o["useEthernet"].isNull()) c.useEthernet = o["useEthernet"];
  if (!o["wifiEnabled"].isNull()) c.wifiEnabled = o["wifiEnabled"];
  if (!o["wifiApMode"].isNull()) c.wifiApMode = o["wifiApMode"];
  if (!o["setupComplete"].isNull()) c.setupComplete = o["setupComplete"];
  if (o["wifiSsid"].is<const char *>()) strlcpy(c.wifiSsid, o["wifiSsid"], sizeof(c.wifiSsid));
  if (o["staSsid"].is<const char *>()) strlcpy(c.staSsid, o["staSsid"], sizeof(c.staSsid));
  // Passwort wird nie ausgeliefert; leeres Feld heißt unverändert
  if (o["staPass"].is<const char *>() && o["staPass"].as<const char *>()[0])
    strlcpy(c.staPass, o["staPass"], sizeof(c.staPass));
  if (o["staSsid"].is<const char *>() && !c.staSsid[0]) c.staPass[0] = '\0';
  // wifiPass / displayCode: random — nicht aus Form überschreiben außer explizit
  if (o["webUser"].is<const char *>()) strlcpy(c.webUser, o["webUser"], sizeof(c.webUser));
  if (o["webPass"].is<const char *>()) strlcpy(c.webPass, o["webPass"], sizeof(c.webPass));
  if (!o["httpsEnabled"].isNull()) c.httpsEnabled = o["httpsEnabled"];
  if (!o["httpsRedirect"].isNull()) c.httpsRedirect = o["httpsRedirect"];
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
  if (!o["gatewayEnabled"].isNull()) c.gatewayEnabled = o["gatewayEnabled"];
  if (!o["gatewayPort"].isNull()) c.gatewayPort = o["gatewayPort"];
  if (!o["displayEnabled"].isNull()) c.displayEnabled = o["displayEnabled"];
  if (!o["displayBrightness"].isNull()) c.displayBrightness = o["displayBrightness"];
  if (!o["displayAutoPage"].isNull()) c.displayAutoPage = o["displayAutoPage"];
  if (!o["displayPageS"].isNull()) c.displayPageMs = constrain(o["displayPageS"].as<uint32_t>(), 3u, 600u) * 1000;
  if (!o["displayTimeoutS"].isNull()) c.displayTimeoutS = constrain(o["displayTimeoutS"].as<uint32_t>(), 0u, 36000u);
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
  // Von außen auf HTTPS umleiten; der HTTPS-Server selbst fragt über 127.0.0.1 an.
  // Während der Einrichtung nicht, da steht man mit dem Handy im Geräte-WLAN.
  server.addMiddleware([](AsyncWebServerRequest *req, ArMiddlewareNext next) {
    const auto &c = ConfigStore::get();
    if (c.httpsRedirect && c.setupComplete && HttpsServer::running() && req->client() &&
        req->client()->remoteIP() != IPAddress(127, 0, 0, 1)) {
      String host = req->host();
      const int colon = host.indexOf(':');
      if (colon >= 0) host = host.substring(0, colon);
      if (!host.length()) host = gLive.ipShown;
      req->redirect("https://" + host + req->url());
      return;
    }
    next();
  });

  // Public stylesheet (no auth) — pages stay readable right after login
  server.on("/a.css", HTTP_GET, [](AsyncWebServerRequest *req) {
    req->send_P(200, "text/css", UI_CSS);
  });
  server.on("/f/barlow-400.woff", HTTP_GET, [](AsyncWebServerRequest *req) {
    auto *r = req->beginResponse(200, "font/woff", FONT_BARLOW_400, FONT_BARLOW_400_LEN);
    r->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    req->send(r);
  });
  server.on("/f/barlow-600.woff", HTTP_GET, [](AsyncWebServerRequest *req) {
    auto *r = req->beginResponse(200, "font/woff", FONT_BARLOW_600, FONT_BARLOW_600_LEN);
    r->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    req->send(r);
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

  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    String out = "{\"uptimeMs\":";
    out += millis();
    out += ",\"hitPolls\":"; out += gLive.hitPolls;
    out += ",\"hitFails\":"; out += gLive.hitFails;
    out += ",\"hitErr\":\"";
    for (const char *p = HitBus::lastError(); *p; p++) if (*p != '"' && *p != '\\') out += *p;
    out += "\",\"events\":";
    out += EventLog::toJson(80);
    out += "}";
    req->send(200, "application/json", out);
  });

  server.on("/api/tls", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send(200, "application/json", HttpsServer::infoJson());
  });

  server.on("/api/tls/cert", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, body)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
              }
              String err;
              JsonDocument out;
              out["ok"] = HttpsServer::installCustom(doc["cert"] | "", doc["key"] | "", err);
              if (err.length()) out["error"] = err;
              String s;
              serializeJson(out, s);
              req->send(out["ok"] ? 200 : 400, "application/json", s);
            });

  server.on("/api/tls/self", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    HttpsServer::useSelfSigned();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/events/clear", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    EventLog::clear();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/setup/finish", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, body)) {
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
              NetBoot::applyConfig(c);
              HitBus::applyConfig(c);
              HitClient::applyConfig(c);
              ModbusGateway::applyConfig(c);
              HitControl::applyConfig(c);
              SunSpecServer::applyConfig(c);
              StatusDisplay::applyConfig(c);
              TasmotaClient::applyConfig(c);
              MqttHa::applyConfig(c);
              HoyCloud::applyConfig(c);
              KnxIp::applyConfig(c);
              HttpsServer::applyConfig(c);
              HitClient::pollNow();
              SunSpecServer::rebuildMap();
              req->send(200, "application/json",
                        (c.httpsEnabled && HttpsServer::running()) ? "{\"ok\":true,\"https\":true}" : "{\"ok\":true}");
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
    doc["resetReason"] = Watchdog::lastResetReason(ConfigStore::get().uiLang == UiLang::En);
    doc["uptimeS"] = millis() / 1000;
    doc["heapInternal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    doc["heapInternalMin"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    doc["https"] = HttpsServer::running();
    doc["gatewayEnabled"] = ConfigStore::get().gatewayEnabled;
    doc["gatewayClients"] = gLive.gatewayClients;
    doc["hitLink"] = ConfigStore::get().hitMode == HitLinkMode::RtuRs485 ? "RS485" : "TCP";
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
    doc["staSsid"] = c.staSsid;
    doc["staPass"] = "";
    doc["staPassSet"] = c.staPass[0] != 0;
    doc["wifiPass"] = c.displayCode[0] ? c.displayCode : c.wifiPass; // UI zeigt Display-Code
    doc["webUser"] = c.webUser;
    doc["webPass"] = c.webPass;
    doc["httpsEnabled"] = c.httpsEnabled;
    doc["httpsRedirect"] = c.httpsRedirect;
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
    doc["gatewayEnabled"] = c.gatewayEnabled;
    doc["gatewayPort"] = c.gatewayPort;
    doc["displayEnabled"] = c.displayEnabled;
    doc["displayBrightness"] = c.displayBrightness;
    doc["displayPageS"] = c.displayPageMs / 1000;
    doc["displayAutoPage"] = c.displayAutoPage;
    doc["displayTimeoutS"] = c.displayTimeoutS;
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
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, body)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
              }
              auto &c = ConfigStore::get();
              applyJson(doc.as<JsonObjectConst>(), c);
              // Speichern = Setup erledigt (QR-Screen aus), außer explizit false
              if (doc["setupComplete"].isNull()) c.setupComplete = true;
              ConfigStore::save();
              gLive.setupScreen = !c.setupComplete;
              NetBoot::applyConfig(c);
              HitBus::applyConfig(c);
              HitClient::applyConfig(c);
              ModbusGateway::applyConfig(c);
              HitControl::applyConfig(c);
              SunSpecServer::applyConfig(c);
              StatusDisplay::applyConfig(c);
              TasmotaClient::applyConfig(c);
              MqttHa::applyConfig(c);
              HoyCloud::applyConfig(c);
              KnxIp::applyConfig(c);
              HttpsServer::applyConfig(c);
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

  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send_P(200, "text/html", HISTORY_HTML);
  });

  server.on("/api/history/day", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    History::day(doc);
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/history/bars", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    const String unit = req->hasParam("unit") ? req->getParam("unit")->value() : String("day");
    const int n = req->hasParam("n") ? constrain(req->getParam("n")->value().toInt(), 1, 400) : 31;
    JsonDocument doc;
    History::bars(unit.c_str(), n, doc);
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/screen", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    size_t len = 0;
    int w = 0, h = 0;
    const uint8_t *fb = StatusDisplay::frame(len, w, h);
    if (!fb) {
      req->send(503, "text/plain", "kein Bildpuffer");
      return;
    }
    auto *r = req->beginResponse("application/octet-stream", len,
                                 [fb, len](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
                                   size_t n = min(maxLen, len - index);
                                   memcpy(buf, fb + index, n);
                                   if (index + n >= len) StatusDisplay::frameDone();
                                   return n;
                                 });
    r->addHeader("X-Width", String(w));
    r->addHeader("X-Height", String(h));
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });

  server.on("/api/access/new-code", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    ConfigStore::newDisplayCode();
    auto &c = ConfigStore::get();
    NetBoot::applyConfig(c);     // Geräte-WLAN mit neuem Passwort neu starten
    StatusDisplay::applyConfig(c);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/display/tap", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    StatusDisplay::requestTap();
    req->send(200, "application/json", "{\"ok\":true}");
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
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument in;
              deserializeJson(in, body);
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
  HttpsServer::loop();
}

} // namespace WebConfig
