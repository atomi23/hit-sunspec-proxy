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
#include "BatteryData.h"
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
#include "DataBroker.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// Board für Status und Funktionsauswahl (Display gibt es nur auf dem T-Connect-Pro)
#if defined(BOARD_T_CONNECT_PRO)
#define BOARD_NAME "T-Connect-Pro"
#define BOARD_HAS_DISPLAY true
#elif defined(BOARD_WT32_ETH01)
#define BOARD_NAME "WT32-ETH01"
#define BOARD_HAS_DISPLAY false
#elif defined(BOARD_P4_POE_ETH)
#define BOARD_NAME "ESP32-P4-POE-ETH"
#define BOARD_HAS_DISPLAY false
#elif defined(BOARD_T_POE_PRO)
#define BOARD_NAME "T-POE-Pro"
#define BOARD_HAS_DISPLAY false
#else
#define BOARD_NAME "ESP32"
#define BOARD_HAS_DISPLAY false
#endif

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
.side nav a.hide{display:none}
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
.cfg #knxRows,.cfg #probeBox,.cfg #tlsBox,.cfg #batBox{column-span:all}
.cfg #features.show{columns:auto}
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

/* KNX-Zuordnung */
.knx-tools{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin:18px 0 6px}
.knx-tools button{padding:7px 14px;font-size:13px}
.knx-wrap{overflow-x:auto}
.knx-tbl{min-width:560px;max-width:860px;border-collapse:separate;border-spacing:0;font-size:14px}
.knx-tbl th{position:sticky;top:0;z-index:1;background:var(--plate);padding:8px 10px;border-bottom:1px solid var(--rule2)}
.knx-tbl td{padding:4px 10px;border-bottom:1px solid var(--rule);vertical-align:middle}
.knx-tbl td:first-child,.knx-tbl th:first-child{width:34px;text-align:center}
.knx-tbl td:nth-child(3),.knx-tbl th:nth-child(3){width:150px}
.knx-tbl td:nth-child(4),.knx-tbl th:nth-child(4){width:170px}
.knx-tbl tr.grp td{text-align:left;padding:16px 10px 6px;font-size:12px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;
  color:var(--dim);border-bottom:1px solid var(--rule2)}
.knx-tbl tr.grp td span{font-weight:400;letter-spacing:0;text-transform:none;margin-left:8px}
.knx-tbl tbody tr.alt td{background:var(--plate2)}
.knx-tbl tbody tr:not(.grp):hover td,.knx-tbl tbody tr:not(.grp):focus-within td{background:color-mix(in srgb,var(--acc) 10%,var(--plate))}
.knx-tbl tr.on td:first-child{box-shadow:inset 3px 0 0 var(--acc)}
.knx-tbl tr:not(.on):not(.grp) .knx-l{color:var(--dim)}
.knx-tbl .knx-l small{display:block;font:400 11px ui-monospace,Consolas,monospace;color:var(--dim)}
.knx-tbl input:not([type=checkbox]),.knx-tbl select{padding:6px 10px;font-size:14px;border-radius:8px}
.knx-tbl input.bad-ga{border-color:var(--fault)}
.knx-tbl tr.imp td{animation:knximp 1.6s ease-out}
@keyframes knximp{from{background:color-mix(in srgb,var(--acc) 30%,var(--plate))}}

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
    <a href="/config#akku"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 12h8"/></svg> Akku</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#features"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="6" height="6" rx="1.5"/><rect x="14" y="4" width="6" height="6" rx="1.5"/><rect x="4" y="14" width="6" height="6" rx="1.5"/><path d="M17 14v6M14 17h6"/></svg> Funktionen</a>
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
  <g id="hemsG">
  <path id="wData" class="data" d="M480 64V166"/>

  <rect id="hemsBox" class="chipbox" x="370" y="24" width="220" height="40" rx="20"/>
  <text x="480" y="49" text-anchor="middle" class="lbl"><tspan style="fill:var(--ink)">Consolinno</tspan><tspan dx="8" id="hemsTxt">wartet</tspan></text>
  </g>

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
// Menüpunkte nicht gewählter Funktionen ausblenden (Einstellungen > Funktionen schaltet sie wieder ein)
function navFeat(F,disp){
  F=F||{};const off={sunspec:F.sunspec===false,mqtt:F.mqtt===false,gateway:F.gateway===false,knx:F.knx===false,
    tasmota:F.tasmota===false,cloud:F.cloud===false,akku:F.akku===false,display:disp===false};
  document.querySelectorAll('.side nav a').forEach(a=>{const m=/#(\w+)$/.exec(a.getAttribute('href')||'');
    if(m&&m[1] in off)a.classList.toggle('hide',off[m[1]]);});
}
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
  if(!window.NAV_FROM_FORM)navFeat(s.features,s.hasDisplay);
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
// Netz: Bezug positiv, Einspeisung mit Minus; Farbe Weiß→Rot / Gelb→Grün
const wattGrid=n=>{const v=Number(n)||0;return (v<0?'−':'')+num(Math.abs(v))+'<tspan dx="6">W</tspan>';};
const gridFill=n=>{
  const v=Number(n)||0,t=Math.min(1,Math.abs(v)/2000);
  const mix=(a,b)=>Math.round(a+(b-a)*t);
  if(v>=0){const r=mix(237,239),g=mix(239,111),b=mix(236,98);return`rgb(${r},${g},${b})`;} // ink→rot
  const r=mix(241,46),g=mix(196,180),b=mix(83,120);return`rgb(${r},${g},${b})`; // gelb→grün
};
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

  const pv=Number(s.pvW)||0, bat=Number(s.batW)||0;
  const useTas=!!(s.tasmotaOk&&s.tasmotaLiveGrid);
  const grid=useTas?(Number(s.tasmotaPowerW)||0):(Number(s.gridW)||0);
  const house=Math.max(0,pv+bat+grid);
  $('vPv').innerHTML=watt(pv);
  $('dPv').textContent=T('today')+' '+num(s.energyPvKwh)+' kWh';
  $('vBat').innerHTML=watt(bat);
  const soc=Math.max(0,Math.min(100,Number(s.soc)||0));
  $('dBat').textContent=(bat>20?T('discharge'):(bat<-20?T('charge'):T('idle')))+', '+num(soc)+' %';
  $('socRing').setAttribute('stroke-dasharray',(326.7*soc/100).toFixed(1)+' 327');
  $('socRing').style.stroke=soc<15?'var(--fault)':(soc<30?'var(--warn)':'var(--acc)');
  $('vGrid').innerHTML=wattGrid(grid);
  $('vGrid').style.fill=gridFill(grid);
  const gridDir=grid>20?T('imp'):(grid<-20?T('exp'):'');
  $('dGrid').textContent=useTas?(gridDir?(T('tas')+' · '+gridDir):T('tas')):gridDir;
  $('vHouse').innerHTML=watt(house);
  run('fPv',pv,true); run('fBat',bat,bat>0); run('fGrid',grid,grid<0); run('fHouse',house,true);
  $('dHit').textContent=s.hitOk?T('hitOk'):T('hitBad');
  $('dHit').style.fill=s.hitOk?'var(--dim)':'var(--fault)';
  $('hemsTxt').textContent=s.sunspecClient?T('hemsOk'):T('hemsWait');
  $('wData').classList.toggle('live',!!s.sunspecClient);
  $('hemsBox').classList.toggle('live',!!s.sunspecClient);
  const F=s.features||{};
  $('hemsG').style.display=F.sunspec===false?'none':'';

  const src={NATIVE:T('srcN'),MQTT:T('srcM'),CLOUD:T('srcC'),DEMO:T('srcD')}[s.dataOrigin]||T('srcX');
  const opt=ok=>ok?'on':'idle';
  // nur Dienste zeigen, die gewählt sind
  const fc=(on,html)=>on===false?'':html;
  $('chips').innerHTML=chip(s.hitOk?'on':'off','HIT')+fc(F.sunspec,chip(opt(s.sunspecClient),T('hems')))+
    chip((s.ethOk||s.wifiOk)?'on':'off',T('net'))+fc(F.mqtt,chip(opt(s.mqttOk),'MQTT')+chip(opt(s.mqttIngestOk),T('mqin')))+
    fc(F.cloud,chip(opt(s.cloudOk),'Cloud'))+fc(F.knx,chip(opt(s.knxOk),'KNX'))+
    (s.gatewayEnabled?chip(opt(s.gatewayClients>0),'Gateway'+(s.gatewayClients?' ('+s.gatewayClients+')':'')):'')+
    `<span class="chip">${T('src')}: <b style="color:var(--ink);font-weight:600">${src}</b></span>`;

  const str=[1,2,3,4].map(i=>[T('string')+' '+i,num(s['pv'+i+'W'])+' W']);
  str.push([T('total'),`<b>${num(pv)} W</b>`]);
  $('tStr').innerHTML=rows(str);
  const phW=w=>{const v=Number(w)||0;const t=(v<0?'−':'')+num(Math.abs(v));return`<span style="color:${gridFill(v)}">${t}</span>`;};
  $('tPh').innerHTML=`<tr><th>${T('phase')}</th><th class="n">V</th><th class="n">A</th><th class="n">W</th></tr>`+
    [1,2,3].map(i=>`<tr><td>L${i}</td><td class="n">${num(s['v'+i],1)}</td><td class="n">${num(s['i'+i],1)}</td><td class="n">${phW(s['gridW'+i])}</td></tr>`).join('');
  $('nHz').textContent=T('freq')+' '+num(s.freqHz,2)+' Hz';
  $('tEn').innerHTML=rows([
    [T('yield'),num(s.energyPvKwh,1)+' kWh'],[T('eimp'),num(s.energyImpKwh,1)+' kWh'],[T('eexp'),num(s.energyExpKwh,1)+' kWh'],
    [T('soc'),num(soc)+' %'],[T('batv'),num(s.batV,1)+' V'],
    [T('bata'),(()=>{const a=-(Number(s.batA)||0);return (a>0?'+':'')+num(a,1)+' A';})()],
    [T('soh'),num(s.soh)+' %']]);
  $('tLim').innerHTML=rows([[T('lim'),s.wMaxLimEna?T('limOn'):T('limOff')],[T('limVal'),num((Number(s.wMaxLimPct)||0)/100,1)+' %']]);
  $('nLim').textContent=s.lastWriteNote||T('noWrite');
  if(s.tasmotaOk){
    const tp=Number(s.tasmotaPowerW)||0;
    $('tTas').innerHTML=rows([[T('power'),`<span style="color:${gridFill(tp)}">${(tp<0?'−':'')+num(Math.abs(tp))} W</span>`],[T('eimp'),num(s.tasmotaTotalKwh,2)+' kWh'],[T('eexp'),num(s.tasmotaExportKwh,2)+' kWh']]);
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
    <a href="/config#akku"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 12h8"/></svg> Akku</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#features"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="6" height="6" rx="1.5"/><rect x="14" y="4" width="6" height="6" rx="1.5"/><rect x="4" y="14" width="6" height="6" rx="1.5"/><path d="M17 14v6M14 17h6"/></svg> Funktionen</a>
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
// Menüpunkte nicht gewählter Funktionen ausblenden (Einstellungen > Funktionen schaltet sie wieder ein)
function navFeat(F,disp){
  F=F||{};const off={sunspec:F.sunspec===false,mqtt:F.mqtt===false,gateway:F.gateway===false,knx:F.knx===false,
    tasmota:F.tasmota===false,cloud:F.cloud===false,akku:F.akku===false,display:disp===false};
  document.querySelectorAll('.side nav a').forEach(a=>{const m=/#(\w+)$/.exec(a.getAttribute('href')||'');
    if(m&&m[1] in off)a.classList.toggle('hide',off[m[1]]);});
}
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
  if(!window.NAV_FROM_FORM)navFeat(s.features,s.hasDisplay);
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
   pv:'PV',house:'Haus',grid:'Netz',bat:'Akku',days:'Tage',months:'Monate',years:'Jahre',total:'Gesamt',
   gen:'Erzeugt',use:'Verbraucht',imp:'Bezug',exp:'Einspeisung',chg:'Akku laden',dis:'Akku entladen',
   aut:'Autarkie',self:'Eigenverbrauch',none:'Noch keine Daten. Der Verlauf beginnt, sobald der ESP Messwerte und die Uhrzeit hat.',
   since:d=>'Aufzeichnung seit '+d+'. Erzeugung, Bezug und Einspeisung aus den HIT-Zählern, Verbrauch und Akku vom ESP aufsummiert.',
   cur:'Laufender Zeitraum',
   bal:'Flächen über der Null-Linie: PV, Netzbezug und Akku entladen. Die Linien zeigen, wohin die Energie geht: rot das Haus, gelb Akku laden. Unter der Null-Linie nur die Einspeisung.',
   pos:'Flächen: PV, Netzbezug und Akku entladen. Linien: rot das Haus, gelb Akku laden, grün die Einspeisung.',
   mBal:'Bilanz',mPos:'Alles positiv'},
 en:{title:'History',sub:'Power over the last 24 hours and energy per day, month and year.',h24:'Last 24 hours',hE:'Energy',
   pv:'Solar',house:'Home',grid:'Grid',bat:'Battery',days:'Days',months:'Months',years:'Years',total:'Total',
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
    <a href="/config#akku"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 12h8"/></svg> Akku</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#features"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="6" height="6" rx="1.5"/><rect x="14" y="4" width="6" height="6" rx="1.5"/><rect x="4" y="14" width="6" height="6" rx="1.5"/><path d="M17 14v6M14 17h6"/></svg> Funktionen</a>
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
<label>Wechselrichter</label>
<select name="inverterType"><option value="0">Hoymiles HIT</option></select>
<p class="hint">Weitere Hersteller sind vorbereitet und folgen.</p>
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
<label><input name="sunspecEnabled" type="checkbox"/> aktiv (SunSpec-Server für das HEMS)</label>
<label>Modbus-Port</label><input name="sunspecPort" type="number"/>
<label>Unit-ID</label><input name="sunspecUnitId" type="number"/>
<label><input name="invertPowerSign" type="checkbox"/> Vorzeichen der Leistung umkehren</label>
<p class="hint">In Consolinno als SunSpec-Gerät eintragen: IP vom Display, Port 1502, Unit-ID 1. Die Registerkarte liegt ab Adresse 0, 40000 und 50000.</p>
</div>
<div class="card" id="features" data-sub="Welche Dienste das Gerät anbietet. Abgewählte bleiben aus und verschwinden aus dem Menü."><strong>Funktionen</strong>
<p class="hint" id="featBoard" style="margin-top:0"></p>
<label><input type="checkbox" data-feat="sunspecEnabled"/> <span><b>Consolinno / SunSpec</b>: HIT als SunSpec-Wechselrichter fürs HEMS</span></label>
<label><input type="checkbox" data-feat="gatewayEnabled"/> <span><b>Modbus-Gateway</b>: andere Programme lesen den HIT über den ESP</span></label>
<label><input type="checkbox" data-feat="mqttEnabled"/> <span><b>Home Assistant</b>: Messwerte per MQTT mit Discovery</span></label>
<label><input type="checkbox" data-feat="knxEnabled"/> <span><b>KNX</b>: Messwerte als Gruppentelegramme</span></label>
<label><input type="checkbox" data-feat="tasmotaEnabled"/> <span><b>Tasmota-Zähler</b>: Netzzähler mit Lesekopf</span></label>
<label><input type="checkbox" data-feat="cloudEnabled"/> <span><b>Hoymiles-Cloud</b>: Ersatzquelle, wenn der HIT ausfällt</span></label>
<label><input type="checkbox" data-feat="batEnabled"/> <span><b>Akkudaten</b>: Temperaturen, Zellspannungen und Grenzwerte vom HIT, optional aus der Akku-Cloud</span></label>
<p class="hint">Die Einstellungen eines Dienstes stehen in seinem Menüpunkt. Speichern übernimmt die Auswahl ohne Neustart.</p>
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
<label>Eigenes Web-Passwort</label><input name="webPass" type="password" placeholder="leer = unverändert / nur Zugangscode"/>
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
<label><input name="tasmotaEnabled" type="checkbox"/> aktiv</label>
<label><input name="tasmotaLiveGrid" type="checkbox"/> Im Live-View als Netzzähler (sonst HIT)</label>
<label><input name="tasmotaMeterSpoof" type="checkbox"/> Meter-Spoof für Consolinno (optional)</label>
<label>Tasmota IP</label><input name="tasmotaHost" placeholder="192.0.2.60"/>
<label>Port</label><input name="tasmotaPort" type="number"/>
<label>Poll (ms)</label><input name="tasmotaPollMs" type="number"/>
<label>User (optional)</label><input name="tasmotaUser"/>
<label>Passwort (optional)</label><input name="tasmotaPass" type="password" placeholder="leer = unverändert"/>
<p class="hint">E320 liefert nur die Summe (Power / E_in / E_out), keine Phasen.
Meter-Spoof hängt SunSpec Model 203 (3ph) an; Phasenleistung ≈ Summe/3, Spannungen vom HIT.</p>
<button class="sec" type="button" onclick="probeTas()" style="margin-top:10px">One-Click: Tasmota auslesen</button>
<div id="probeBox"><div id="probeSum"></div><pre id="probeRaw"></pre></div>
</div>
<div class="card" id="mqtt" data-sub="Messwerte per MQTT an Home Assistant, optional auch als Datenquelle."><strong>Home Assistant</strong>
<label><input name="mqttEnabled" type="checkbox"/> Publish aktiv</label>
<label><input name="mqttDiscovery" type="checkbox"/> HA Discovery</label>
<label>Broker IP</label><input name="mqttHost" placeholder="192.0.2.10"/>
<label>Port</label><input name="mqttPort" type="number"/>
<label>User</label><input name="mqttUser"/>
<label>Passwort</label><input name="mqttPass" type="password" placeholder="leer = unverändert"/>
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
<label>Passwort</label><input name="cloudPass" type="password" placeholder="leer = unverändert"/>
<label>Station / Plant ID (sid)</label><input name="cloudStationId" placeholder="z.B. 1234567"/>
<label>Poll (ms)</label><input name="cloudPollMs" type="number"/>
<label>Max Alter (ms)</label><input name="cloudMaxAgeMs" type="number"/>
<p class="hint">Inoffizielle S-Miles API — nur Lesen. Schreiben (§14a) nur über HIT Modbus.</p>
</div>
<div class="card" id="akku" data-sub="Zusätzliche Akkuwerte vom HIT und aus der Cloud des Akku-Herstellers. Nur lesen."><strong>Akku</strong>
<label><input name="batEnabled" type="checkbox"/> aktiv</label>
<label><input name="batHitEnabled" type="checkbox"/> Werte vom HIT lesen (jeder Hersteller, soweit das BMS sie liefert)</label>
<label>HIT-Abfrage alle (s)</label><input name="batHitPollS" type="number" min="30" max="3600"/>
<label>Akku-Cloud</label>
<select name="batCloud"><option value="0">keine</option><option value="1">Dyness</option></select>
<label>API Host</label><input name="batCloudHost" placeholder="open-api.dyness.com"/>
<label>App-ID</label><input name="batCloudAppId" autocomplete="off"/>
<label>App-Secret</label><input name="batCloudSecret" type="password" autocomplete="new-password" placeholder="leer lassen = unverändert"/>
<label>Seriennummer</label><input name="batCloudSn" autocomplete="off" placeholder="leer = automatisch das erste Gerät"/>
<label>Cloud-Abfrage alle (s)</label><input name="batCloudPollS" type="number" min="300" max="3600"/>
<p class="hint">App-ID und Secret legt man im Dyness-Portal ems.dyness.com unter Entwicklerzentrum, API-Verwaltung an. Das Secret wird dort nur einmal angezeigt. Nutzt ein anderes Programm (z. B. die Home-Assistant-Integration) dieselbe App-ID, teilen sich beide die Abrufgrenze von Dyness.</p>
<div id="batBox"></div>
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
<p class="hint">Pro Zeile: Haken setzen, Gruppenadresse dreistufig <code>H/M/U</code> (z.B. <code>1/2/10</code>), Datenpunkttyp ggf. anpassen. Rot markiert = ungültig oder doppelt.</p>
<div id="knxRows">
<div class="knx-tools">
<button type="button" class="sec" onclick="knxExport('xml')" title="Gruppenadressen-Export im ETS-Format, in ETS 5/6 importierbar">Export ETS (XML)</button>
<button type="button" class="sec" onclick="knxExport('csv')" title="ETS-CSV (1/1, Semikolon)">Export CSV</button>
<button type="button" class="sec" onclick="document.getElementById('knxFile').click()" title="ETS-Gruppenadressen-Export (XML oder CSV)">Import XML/CSV</button>
<input type="file" id="knxFile" accept=".xml,.csv,.txt" hidden onchange="knxImport(this)"/>
<span class="meta" id="knxMsg"></span>
</div>
<div class="knx-wrap"><table class="knx-tbl"><thead><tr><th title="senden">An</th><th>Messwert</th><th>Gruppenadresse</th><th>Datenpunkttyp</th></tr></thead>
<tbody id="knxBody"></tbody></table></div>
<p class="hint">Export: aktive Zeilen mit gültiger Adresse. Import: ETS → Gruppenadressen → Export (XML oder CSV).
Zugeordnet wird über die Beschreibung <code>hit-sunspec-proxy:&lt;id&gt;</code> (steht in jedem Export), sonst über den Namen.
Danach Speichern.</p>
</div>
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
// Menüpunkte nicht gewählter Funktionen ausblenden (Einstellungen > Funktionen schaltet sie wieder ein)
function navFeat(F,disp){
  F=F||{};const off={sunspec:F.sunspec===false,mqtt:F.mqtt===false,gateway:F.gateway===false,knx:F.knx===false,
    tasmota:F.tasmota===false,cloud:F.cloud===false,akku:F.akku===false,display:disp===false};
  document.querySelectorAll('.side nav a').forEach(a=>{const m=/#(\w+)$/.exec(a.getAttribute('href')||'');
    if(m&&m[1] in off)a.classList.toggle('hide',off[m[1]]);});
}
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
  if(!window.NAV_FROM_FORM)navFeat(s.features,s.hasDisplay);
}
async function saveUiPrefs(){
  const body={uiLang:document.getElementById('uiLang').value,uiTheme:document.getElementById('uiTheme').value};
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  location.reload();
}
</script>
<script>
const KNX_DPT={PowerW:[[14056,'14.056 W'],[9024,'9.024 kW']],Percent:[[5001,'5.001 %'],[9007,'9.007 %']],Volt:[[14027,'14.027 V'],[9001,'9.001']],Amp:[[14019,'14.019 A'],[9001,'9.001']],Hz:[[14033,'14.033 Hz'],[9001,'9.001']],EnergyKwh:[[13010,'13.010 Wh'],[14019,'14.x Float kWh']],RatioPct:[[5001,'5.001 %'],[9007,'9.007 %']]};
let KNX_META=[];
function dptOpts(kind,cur){
  return (KNX_DPT[kind]||KNX_DPT.PowerW).map(([v,l])=>`<option value="${v}" ${Number(cur)===v?'selected':''}>${l}</option>`).join('');
}
function knxGrp(id){
  if(/^pv/.test(id))return 'PV';
  if(/^(bat|soc|soh)/.test(id))return 'Batterie';
  if(/^(grid|v\d|i\d|freq)/.test(id))return 'Netz';
  if(/^e/.test(id))return 'Energie HIT';
  if(/^tas/.test(id))return 'Tasmota-Zähler';
  return 'Consolinno';
}
const knxEsc=s=>String(s??'').replace(/[&<>"']/g,x=>'&#'+x.charCodeAt(0)+';');
function gaParse(s){const m=/^\s*(\d{1,2})\/(\d)\/(\d{1,3})\s*$/.exec(String(s||''));if(!m)return -1;
  const a=+m[1],b=+m[2],c=+m[3];return(a>31||b>7||c>255)?-1:(a<<11)|(b<<8)|c;}
const gaStr=n=>`${n>>11}/${(n>>8)&7}/${n&255}`;
function knxEl(k,i){return document.querySelector(`#knxBody [data-knx=${k}][data-i="${i}"]`);}
function renderKnx(meta,ch){
  KNX_META=meta;
  const body=document.getElementById('knxBody'); if(!body)return;
  let html='',last='',alt=false;
  meta.forEach((m,i)=>{
    const g=knxGrp(m.id);
    if(g!==last){html+=`<tr class="grp"><td colspan="4">${knxEsc(g)}<span></span></td></tr>`;last=g;alt=false;}
    const c=(ch&&ch[i])||{};
    html+=`<tr data-row="${i}" class="${c.ena?'on':''}${alt?' alt':''}">
      <td><input type="checkbox" data-knx="e" data-i="${i}" ${c.ena?'checked':''} aria-label="${knxEsc(m.label)} senden"/></td>
      <td class="knx-l">${knxEsc(m.label)}<small>${knxEsc(m.id)}</small></td>
      <td><input data-knx="g" data-i="${i}" type="text" autocomplete="off" spellcheck="false" placeholder="z.B. 1/2/${10+i}" value="${knxEsc(c.ga)}" aria-label="Gruppenadresse ${knxEsc(m.label)}"/></td>
      <td><select data-knx="d" data-i="${i}" aria-label="Datenpunkttyp ${knxEsc(m.label)}">${dptOpts(m.kind,c.dpt||m.dpt)}</select></td></tr>`;
    alt=!alt;
  });
  body.innerHTML=html;
  knxCheck();
}
// Markiert ungültige, fehlende und doppelte Adressen und zählt aktive Zeilen je Gruppe.
function knxCheck(){
  const rows=[...document.querySelectorAll('#knxBody tr[data-row]')];
  const seen={};
  rows.forEach(r=>{const n=gaParse(r.querySelector('[data-knx=g]').value);
    if(n>=0&&r.querySelector('[data-knx=e]').checked)seen[n]=(seen[n]||0)+1;});
  rows.forEach(r=>{
    const e=r.querySelector('[data-knx=e]'),g=r.querySelector('[data-knx=g]');const v=g.value.trim(),n=gaParse(v);
    r.classList.toggle('on',e.checked);
    let msg='';
    if(v&&n<0)msg='Ungültig, Format H/M/U (0-31/0-7/0-255)';
    else if(e.checked&&!v)msg='Aktiv, aber ohne Gruppenadresse';
    else if(e.checked&&seen[n]>1)msg='Adresse mehrfach vergeben';
    g.classList.toggle('bad-ga',!!msg);g.title=msg||'Gruppenadresse H/M/U';
  });
  document.querySelectorAll('#knxBody tr.grp').forEach(gr=>{
    let on=0,all=0;
    for(let r=gr.nextElementSibling;r&&!r.classList.contains('grp');r=r.nextElementSibling){all++;if(r.classList.contains('on'))on++;}
    gr.querySelector('span').textContent=`${on} von ${all} aktiv`;
  });
}
{const kb=document.getElementById('knxBody');
 if(kb)['input','change'].forEach(ev=>kb.addEventListener(ev,e=>{const t=e.target;
   // erste gültige Adresse in einer leeren Zeile setzt den Haken einmalig mit
   if(t.dataset.knx==='g'&&!t.defaultValue&&!t.dataset.auto&&gaParse(t.value)>=0){const cb=knxEl('e',t.dataset.i);if(cb&&!cb.checked)cb.checked=true;t.dataset.auto=1;}
   knxCheck();}));}
function collectKnx(){
  return KNX_META.map((m,i)=>{const e=knxEl('e',i),g=knxEl('g',i),d=knxEl('d',i);
    return {ena:!!(e&&e.checked),ga:g?g.value.trim():'',dpt:d?Number(d.value):0};});
}
function knxMsg(t,ok){const m=document.getElementById('knxMsg');m.textContent=t;m.style.color=ok?'var(--acc)':'var(--fault)';}
function dptEts(kind,dpt){return (kind==='EnergyKwh'&&dpt===14019)?'DPT-14':`DPST-${Math.floor(dpt/1000)}-${dpt%1000}`;}
function knxDownload(name,text,type){
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([text],{type}));a.download=name;
  document.body.appendChild(a);a.click();setTimeout(()=>{URL.revokeObjectURL(a.href);a.remove();},0);
}
const KNX_TAG='hit-sunspec-proxy:';
// Export im Format von ETS "Gruppenadressen exportieren" (XML ga-export/01 bzw. CSV 1/1)
function knxExport(fmt){
  const rows=collectKnx().map((c,i)=>({m:KNX_META[i],ena:c.ena,n:gaParse(c.ga),dpt:c.dpt}))
    .filter(r=>r.ena&&r.n>=0).sort((a,b)=>a.n-b.n);
  if(!rows.length){knxMsg('Keine aktive Zeile mit gültiger Gruppenadresse',false);return;}
  const tree=new Map();
  rows.forEach(r=>{const h=r.n>>11,mi=(r.n>>8)&7;if(!tree.has(h))tree.set(h,new Map());
    const t=tree.get(h);if(!t.has(mi))t.set(mi,[]);t.get(mi).push(r);});
  const midName=list=>{const g=new Set(list.map(r=>knxGrp(r.m.id)));return g.size===1?[...g][0]:'Messwerte';};
  let out;
  if(fmt==='xml'){
    const x=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&apos;'}[c]));
    out='<?xml version="1.0" encoding="utf-8" standalone="yes"?>\n<GroupAddress-Export xmlns="http://knx.org/xml/ga-export/01">\n';
    tree.forEach((mids,h)=>{
      out+=`  <GroupRange Name="HIT-Proxy" RangeStart="${h<<11}" RangeEnd="${(h<<11)+2047}">\n`;
      mids.forEach((list,mi)=>{const s=(h<<11)|(mi<<8);
        out+=`    <GroupRange Name="${x(midName(list))}" RangeStart="${s}" RangeEnd="${s+255}">\n`;
        list.forEach(r=>{out+=`      <GroupAddress Name="${x(r.m.label)}" Address="${gaStr(r.n)}" Description="${x(KNX_TAG+r.m.id)}" DPTs="${dptEts(r.m.kind,r.dpt)}" />\n`;});
        out+='    </GroupRange>\n';});
      out+='  </GroupRange>\n';});
    out+='</GroupAddress-Export>\n';
    knxDownload('hit-proxy-knx.xml',out,'application/xml');
  }else{
    const line=a=>a.map(s=>'"'+String(s).replace(/"/g,'""')+'"').join(';')+'\r\n';
    out='\ufeff'+line(['Group name','Address','Central','Unfiltered','Description','DatapointType','Security']);
    tree.forEach((mids,h)=>{out+=line(['HIT-Proxy',`${h}/-/-`,'','','','','Auto']);
      mids.forEach((list,mi)=>{out+=line([midName(list),`${h}/${mi}/-`,'','','','','Auto']);
        list.forEach(r=>{out+=line([r.m.label,gaStr(r.n),'','',KNX_TAG+r.m.id,dptEts(r.m.kind,r.dpt),'Auto']);});});});
    knxDownload('hit-proxy-knx.csv',out,'text/csv');
  }
  knxMsg(rows.length+' Adressen exportiert',true);
}
function knxDecode(buf){
  const u=new Uint8Array(buf);
  if(u[0]===0xFF&&u[1]===0xFE)return new TextDecoder('utf-16le').decode(buf);
  if(u[0]===0xFE&&u[1]===0xFF)return new TextDecoder('utf-16be').decode(buf);
  try{return new TextDecoder('utf-8',{fatal:true}).decode(buf);}catch(e){return new TextDecoder('windows-1252').decode(buf);}
}
function knxParseXml(t){
  const d=new DOMParser().parseFromString(t,'application/xml');
  if(d.getElementsByTagName('parsererror').length)throw new Error('kein gültiges XML');
  return [...d.getElementsByTagName('GroupAddress')].map(g=>{
    let a=g.getAttribute('Address')||'';
    if(/^\d+$/.test(a))a=gaStr(+a&0xFFFF); // .knxproj speichert die Adresse als Zahl
    return {name:g.getAttribute('Name')||'',addr:a,desc:g.getAttribute('Description')||'',
      dpt:g.getAttribute('DPTs')||g.getAttribute('DatapointType')||''};
  });
}
function knxCsvRows(t){
  const first=t.split(/\r?\n/,1)[0];
  const sep=[';','\t',','].map(s=>[s,first.split(s).length]).sort((a,b)=>b[1]-a[1])[0][0];
  const rows=[];let row=[],f='',q=false;
  for(let i=0;i<t.length;i++){const c=t[i];
    if(q){if(c==='"'){if(t[i+1]==='"'){f+='"';i++;}else q=false;}else f+=c;}
    else if(c==='"')q=true;
    else if(c===sep){row.push(f);f='';}
    else if(c==='\n'||c==='\r'){if(c==='\r'&&t[i+1]==='\n')i++;row.push(f);rows.push(row);row=[];f='';}
    else f+=c;}
  if(f||row.length){row.push(f);rows.push(row);}
  return rows;
}
function knxParseCsv(t){
  const rows=knxCsvRows(t).filter(r=>r.some(c=>c.trim()));
  if(!rows.length)return [];
  const isGa=s=>gaParse(s)>=0;
  const hdr=rows[0].some(isGa)?null:rows.shift().map(h=>h.trim().toLowerCase());
  const col=re=>hdr?hdr.findIndex(h=>re.test(h)):-1;
  const ai=col(/^(address|adresse)$/),ni=col(/^(group name|name|sub|gruppenname|untergruppe)$/),
        di=col(/descr|beschreib/),pi=col(/datapoint|dpt|datenpunkt/);
  return rows.map(r=>{
    const a=ai>=0?r[ai]:r.find(isGa);if(!a||!isGa(a))return null;
    const idx=ai>=0?ai:r.indexOf(a);
    let name=ni>=0?(r[ni]||''):'';
    if(!name.trim())name=r.slice(0,idx).reverse().find(c=>c.trim())||'';
    return {name:name.trim(),addr:a.trim(),desc:di>=0?(r[di]||''):'',dpt:pi>=0?(r[pi]||''):''};
  }).filter(Boolean);
}
// "DPST-14-56", "DPT-14", "14.056" -> passende Option für die Messgröße, sonst 0 (unverändert)
function knxPickDpt(kind,s){
  s=String(s||'').trim().split(/[,\s]+/)[0];
  let main=-1,dpt=0,m;
  if((m=/^DPST-(\d+)-(\d+)$/i.exec(s)||/^(\d+)\.(\d+)$/.exec(s))){main=+m[1];dpt=main*1000+(+m[2]);}
  else if((m=/^DPT-(\d+)$/i.exec(s))){main=+m[1];}
  else return 0;
  const opts=(KNX_DPT[kind]||[]).map(o=>o[0]);
  if(dpt&&opts.includes(dpt))return dpt;
  const same=opts.filter(v=>Math.floor(v/1000)===main);
  return same.length===1?same[0]:0;
}
async function knxImport(inp){
  const f=inp.files&&inp.files[0];inp.value='';if(!f)return;
  let list;
  try{const t=knxDecode(await f.arrayBuffer());list=/^\s*</.test(t)?knxParseXml(t):knxParseCsv(t);}
  catch(e){knxMsg('Datei nicht lesbar: '+e.message,false);return;}
  if(!list.length){knxMsg('Keine Gruppenadressen in der Datei gefunden',false);return;}
  const norm=s=>String(s||'').toLowerCase().replace(/[^a-z0-9äöüß%]+/g,'');
  const byId=new Map(KNX_META.map((m,i)=>[m.id,i])),byName=new Map(KNX_META.map((m,i)=>[norm(m.label),i]));
  const done=new Set();let miss=0;
  list.forEach(g=>{
    const tm=/hit-sunspec-proxy:([a-z0-9_]+)/i.exec(g.desc);
    let i=tm?byId.get(tm[1].toLowerCase()):undefined;
    if(i===undefined)i=byName.get(norm(g.name));
    if(i===undefined)i=byId.get(g.name.trim().toLowerCase());
    const n=gaParse(g.addr);
    if(i===undefined||n<0){miss++;return;}
    knxEl('g',i).value=gaStr(n);knxEl('e',i).checked=true;
    const d=knxPickDpt(KNX_META[i].kind,g.dpt);if(d)knxEl('d',i).value=String(d);
    done.add(i);
  });
  done.forEach(i=>{const r=knxEl('e',i).closest('tr');r.classList.remove('imp');void r.offsetWidth;r.classList.add('imp');});
  knxCheck();
  if(!done.size){knxMsg(`Keine der ${list.length} Adressen passt zu einem Messwert`,false);return;}
  knxMsg(`${done.size} Messwerte zugeordnet`+(miss?`, ${miss} Adressen ohne Zuordnung`:'')+'. Zum Übernehmen Speichern.',true);
}
// Funktionen: Spiegel der "aktiv"-Schalter der einzelnen Dienste; das Menü folgt dem Formular
window.NAV_FROM_FORM=true;
let CFG_LOADED=false,HAS_DISPLAY=true;
const FEAT_KEY={sunspecEnabled:'sunspec',gatewayEnabled:'gateway',mqttEnabled:'mqtt',knxEnabled:'knx',tasmotaEnabled:'tasmota',cloudEnabled:'cloud',batEnabled:'akku'};
function formFeat(){
  const f=document.getElementById('f');const g=n=>{const e=f.elements.namedItem(n);return !!(e&&e.checked);};
  return {sunspec:g('sunspecEnabled'),gateway:g('gatewayEnabled'),mqtt:g('mqttEnabled')||g('mqttIngestEnabled'),
    knx:g('knxEnabled'),tasmota:g('tasmotaEnabled'),cloud:g('cloudEnabled'),akku:g('batEnabled')};
}
function syncFeat(){
  const F=formFeat();
  document.querySelectorAll('[data-feat]').forEach(cb=>{cb.checked=F[FEAT_KEY[cb.dataset.feat]];});
  navFeat(F,HAS_DISPLAY);
}
document.querySelectorAll('[data-feat]').forEach(cb=>cb.addEventListener('change',()=>{
  const f=document.getElementById('f'),k=cb.dataset.feat;
  f.elements.namedItem(k).checked=cb.checked;
  if(k==='mqttEnabled'&&!cb.checked){const i=f.elements.namedItem('mqttIngestEnabled');if(i)i.checked=false;}
  syncFeat();
}));
document.getElementById('f').addEventListener('change',e=>{if(e.target.name&&CFG_LOADED)syncFeat();});
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
  renderKnx(meta.points||[], c.knxCh||[]); CFG_LOADED=true; syncFeat(); refresh(); route(); loadTls();
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
  await save();
  const start=await(await fetch('/api/control/apply',{method:'POST'})).json();
  let r=start;
  for(let i=0;i<40&&(r.pending||!r.done);i++){await new Promise(x=>setTimeout(x,250));r=await(await fetch('/api/control/result')).json();}
  alert(r.ok?'An HIT geschrieben':('Fehler: '+(r.error||'timeout')));
}
async function probeTas(){
  const f=document.getElementById('f');
  const body={tasmotaHost:f.tasmotaHost.value,tasmotaPort:Number(f.tasmotaPort.value),
    tasmotaUser:f.tasmotaUser.value,tasmotaPass:f.tasmotaPass.value};
  document.getElementById('probeBox').style.display='block';
  document.getElementById('probeSum').textContent='lese…';
  document.getElementById('probeSum').style.color='var(--dim)';
  document.getElementById('probeRaw').textContent='';
  const start=await(await fetch('/api/tasmota/probe',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).json();
  if(!start.pending&&!start.ok){document.getElementById('probeSum').textContent=start.summary||'Fehler';document.getElementById('probeSum').style.color='var(--fault)';return;}
  let r=start;
  for(let i=0;i<40&&r.pending;i++){await new Promise(x=>setTimeout(x,250));r=await(await fetch('/api/tasmota/probe/result')).json();}
  document.getElementById('probeSum').textContent=r.summary||(r.ok?'OK':'Fehler');
  document.getElementById('probeSum').style.color=r.ok?'var(--acc)':'var(--fault)';
  document.getElementById('probeRaw').textContent=r.raw||'';
  if(r.ok){f.tasmotaEnabled.checked=true;refresh();}
}
async function refresh(){const s=await(await fetch('/api/status')).json();
applyThemeLang(s);
HAS_DISPLAY=s.hasDisplay!==false;
const fb=document.getElementById('featBoard');if(fb)fb.textContent='Board: '+(s.board||'-')+', Wechselrichter: Hoymiles HIT';
if(CFG_LOADED)syncFeat();
const chip=(st,l)=>`<span class="chip ${st}"><span class="dot"></span>${l}</span>`;
document.getElementById('chips').innerHTML=chip(s.hitOk?'on':'off','HIT')+((s.features||{}).sunspec===false?'':chip(s.sunspecClient?'on':'idle','Consolinno'))+
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
<p class="sub">Hoymiles HIT → dieser Proxy → Consolinno, Home Assistant, KNX …</p>
<div class="stepper" id="stepLabels"></div>
<div class="steps" id="bar"></div>

<div class="card" id="s0" data-label="Willkommen">
<span class="pill"></span>
<strong style="margin-top:10px">Willkommen</strong>
<p class="hint">Der Proxy hält die einzige Verbindung zum Wechselrichter und verteilt die Werte: als SunSpec-Wechselrichter
an Consolinno, an Home Assistant, KNX oder andere Modbus-Programme. Welche Dienste laufen, wählst du gleich aus.</p>
<ul>
<li>Display zeigt die Proxy-IP (Ethernet bevorzugt), falls das Board eines hat</li>
<li>Alles lässt sich später unter Einstellungen ändern</li>
</ul>
<div class="row"><button type="button" onclick="next()">Weiter</button></div>
</div>

<div class="card hide" id="s1" data-label="Zugang">
<span class="pill"></span>
<strong style="margin-top:10px">Zugang</strong>
<p class="hint">Display-Code (Login-Fallback, auf dem Display):</p>
<div class="ip" id="dcode">…</div>
<label>Web-User</label><input id="webUser" value="admin"/>
<label>Eigenes Web-Passwort (gemerkt)</label><input id="webPass" type="password" placeholder="empfohlen"/>
<p class="hint">Login danach: User + Passwort <b>oder</b> Display-Code.</p>
<div class="row"><button class="sec" type="button" onclick="back()">Zurück</button><button type="button" onclick="next()">Weiter</button></div>
</div>

<div class="card hide" id="s2" data-label="Funktionen">
<span class="pill"></span>
<strong style="margin-top:10px">Was soll das Gerät tun?</strong>
<p class="hint" id="boardInfo"></p>
<label><input id="w_sunspec" type="checkbox"/> <span><b>Consolinno / SunSpec</b>: HIT als SunSpec-Wechselrichter fürs HEMS (Port 1502)</span></label>
<label><input id="w_gateway" type="checkbox"/> <span><b>Modbus-Gateway</b>: andere Programme lesen den HIT über den ESP</span></label>
<label><input id="w_mqtt" type="checkbox"/> <span><b>Home Assistant</b>: Messwerte per MQTT mit Discovery</span></label>
<label><input id="w_knx" type="checkbox"/> <span><b>KNX</b>: Messwerte als Gruppentelegramme</span></label>
<label><input id="w_tasmota" type="checkbox"/> <span><b>Tasmota-Zähler</b>: Netzzähler mit Lesekopf</span></label>
<label><input id="w_cloud" type="checkbox"/> <span><b>Hoymiles-Cloud</b>: Ersatzquelle, wenn der HIT ausfällt</span></label>
<label><input id="w_akku" type="checkbox"/> <span><b>Akkudaten</b>: Temperaturen, Zellspannungen und Grenzwerte vom HIT</span></label>
<p class="hint">Abgewählte Dienste starten nicht und erscheinen nicht im Menü. Unter Einstellungen, Funktionen lassen sie sich jederzeit einschalten.</p>
<div class="row"><button class="sec" type="button" onclick="back()">Zurück</button><button type="button" onclick="next()">Weiter</button></div>
<p class="hint" id="featMsg"></p>
</div>

<div class="card hide" id="s3" data-label="Wechselrichter">
<span class="pill"></span>
<strong style="margin-top:10px">Wechselrichter</strong>
<label>Hersteller / Modell</label>
<select id="inverterType"><option value="0">Hoymiles HIT</option></select>
<p class="hint">Weitere Hersteller sind vorbereitet und folgen.</p>
<label><input id="demoMode" type="checkbox"/> Erst nur Demo (Fake-Daten, kein Wechselrichter)</label>
<label>IP-Adresse</label><input id="hitHost" placeholder="z.B. 192.168.x.x"/>
<label>Port</label><input id="hitPort" type="number" value="502"/>
<label>Unit-ID</label><input id="hitUnitId" type="number" value="1"/>
<div class="row">
<button class="sec" type="button" onclick="back()">Zurück</button>
<button class="sec" type="button" onclick="testHit()">Verbindung testen</button>
<button type="button" onclick="next()">Weiter</button>
</div>
<p class="hint" id="hitMsg"></p>
</div>

<div class="card hide" id="s4" data-label="Consolinno">
<span class="pill"></span>
<strong style="margin-top:10px">Consolinno anbinden</strong>
<p class="hint">In der Consolinno-App „Fronius Hybrid (SunSpec)“ oder „SunSpec Generic“ wählen.
IP = diese Proxy-Adresse, Port = <b>1502</b> (SunSpec). Automatischer Scan auf 502 findet den Proxy nur, wenn SunSpec dort lauscht — Standard hier ist 1502.</p>
<div class="ip" id="proxyIp">…</div>
<ul>
<li>SunSpec-Port am Proxy: <code id="ssPort">1502</code></li>
<li>Unit / Slave = <code>1</code></li>
</ul>
<p class="hint">Den HIT nicht zusätzlich direkt in Consolinno eintragen. Modbus-Gateway (:502) dann auslassen oder auf einen anderen Port legen.</p>
<div class="row"><button class="sec" type="button" onclick="back()">Zurück</button><button type="button" onclick="next()">Weiter</button></div>
</div>

<div class="card hide" id="s5" data-label="Fertig">
<span class="pill"></span>
<strong style="margin-top:10px">Fertig — autark</strong>
<p class="hint">Speichern schaltet den Setup-Screen am Display aus. Danach laufen diese Dienste:</p>
<ul id="sumList"></ul>
<p class="hint" id="sumTodo"></p>
<div class="row">
<button class="sec" type="button" onclick="back()">Zurück</button>
<button type="button" onclick="finish()">Einrichtung abschließen</button>
</div>
<p class="hint" id="finMsg"></p>
</div>
</div>
<script>
const FEATS=[['sunspec','sunspecEnabled','Consolinno / SunSpec','Consolinno'],['gateway','gatewayEnabled','Modbus-Gateway','Modbus-Gateway'],
  ['mqtt','mqttEnabled','Home Assistant (MQTT)','Home Assistant'],['knx','knxEnabled','KNX','KNX'],
  ['tasmota','tasmotaEnabled','Tasmota-Zähler','Tasmota-Zähler'],['cloud','cloudEnabled','Hoymiles-Cloud','Hoymiles-Cloud'],
  ['akku','batEnabled','Akkudaten','Akkudaten']];
const $w=id=>document.getElementById(id);
const feat=k=>$w('w_'+k).checked;
// Consolinno-Schritt nur, wenn SunSpec gewählt ist
const visible=()=>[0,1,2,3,4,5].filter(i=>i!==4||feat('sunspec'));
let step=0;
function paint(){
  const vis=visible();
  if(!vis.includes(step))step=vis[vis.length-1];
  const pos=vis.indexOf(step);
  $w('stepLabels').innerHTML=vis.map((i,n)=>`<span class="${n===pos?'on':(n<pos?'done':'')}">${$w('s'+i).dataset.label}</span>`).join('');
  $w('bar').innerHTML=vis.map((i,n)=>`<i class="${n<=pos?'on':''}"></i>`).join('');
  for(let i=0;i<6;i++){
    const c=$w('s'+i);c.classList.toggle('hide',i!==step);
    c.querySelector('.pill').textContent='Schritt '+(vis.indexOf(i)+1)+' / '+vis.length;
  }
  if(step===5)summary();
}
function next(){
  if(step===2&&!FEATS.some(([k])=>feat(k))){$w('featMsg').innerHTML='<span class="bad">Mindestens eine Funktion wählen</span>';return;}
  $w('featMsg').textContent='';
  const vis=visible();step=vis[Math.min(vis.indexOf(step)+1,vis.length-1)];paint();
}
function back(){const vis=visible();step=vis[Math.max(vis.indexOf(step)-1,0)];paint();}
function summary(){
  const on=FEATS.filter(([k])=>feat(k));
  $w('sumList').innerHTML=on.map(([,,l])=>`<li>${l}</li>`).join('');
  const todo=on.filter(([k])=>k!=='sunspec').map(([,,,m])=>m);
  $w('sumTodo').textContent=todo.length?('Danach unter Einstellungen einrichten: '+todo.join(', ')+'.'):'';
}
async function boot(){
  const [c,s]=await Promise.all([fetch('/api/config').then(r=>r.json()),fetch('/api/status').then(r=>r.json())]);
  if(c.setupComplete){location.href='/';return;}
  $w('dcode').textContent=c.displayCode||c.wifiPass||'—';
  $w('webUser').value=c.webUser||'admin';
  $w('hitHost').value=(c.hitHost&&!c.hitHost.startsWith('192.0.2'))?c.hitHost:'';
  $w('hitPort').value=c.hitPort||502;
  $w('hitUnitId').value=c.hitUnitId||1;
  $w('inverterType').value=String(c.inverterType||0);
  $w('demoMode').checked=!!c.demoMode;
  $w('proxyIp').textContent=s.ipShown||'—';
  $w('ssPort').textContent=c.sunspecPort||1502;
  $w('boardInfo').textContent='Board: '+(s.board||'-')+(s.hasDisplay?', mit Display':', ohne Display');
  for(const [k,key] of FEATS)$w('w_'+k).checked=key==='mqttEnabled'?!!(c.mqttEnabled||c.mqttIngestEnabled):!!c[key];
  FEATS.forEach(([k])=>$w('w_'+k).addEventListener('change',paint));
  paint();
}
async function testHit(){
  const msg=$w('hitMsg');
  if($w('demoMode').checked){msg.innerHTML='<span class="ok">Demo gewählt — Test übersprungen.</span>';return;}
  const body={hitHost:$w('hitHost').value.trim(),hitPort:Number($w('hitPort').value),
    hitUnitId:Number($w('hitUnitId').value),inverterType:Number($w('inverterType').value),demoMode:false,hitMode:0,setupComplete:false};
  if(!body.hitHost){msg.innerHTML='<span class="bad">IP-Adresse fehlt</span>';return;}
  msg.textContent='Speichern & pollen…';
  const s0=await(await fetch('/api/status')).json();
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  await fetch('/api/poll',{method:'POST'});
  // die Abfrage läuft im Hintergrund; auf ihr Ergebnis warten (max. ~15 s)
  let s=s0;
  for(let i=0;i<30;i++){await new Promise(x=>setTimeout(x,500));s=await(await fetch('/api/status')).json();if(s.hitPollsDone>s0.hitPollsDone)break;}
  msg.innerHTML=s.hitOk?('<span class="ok">Wechselrichter ok — '+s.pvW+' W PV, SoC '+s.soc+' %</span>'):('<span class="bad">Keine Antwort — IP/Netz prüfen ('+(s.status||'')+')</span>');
}
async function finish(){
  const fin=$w('finMsg'); fin.textContent='Speichere…';
  const body={
    webUser:$w('webUser').value.trim()||'admin',
    webPass:$w('webPass').value,
    inverterType:Number($w('inverterType').value)||0,
    hitHost:$w('hitHost').value.trim(),
    hitPort:Number($w('hitPort').value)||502,
    hitUnitId:Number($w('hitUnitId').value)||1,
    hitMode:0,
    demoMode:$w('demoMode').checked,
    sunspecPort:1502,sunspecUnitId:1,
    setupComplete:true,wifiApMode:false
  };
  for(const [k,key] of FEATS)body[key]=feat(k);
  if(!feat('mqtt'))body.mqttIngestEnabled=false;
  if(!body.demoMode && !body.hitHost){fin.innerHTML='<span class="bad">IP-Adresse oder Demo nötig</span>';step=3;paint();return;}
  const r=await(await fetch('/api/setup/finish',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).json();
  if(!r.ok){fin.innerHTML='<span class="bad">'+(r.error||'Fehler')+'</span>';return;}
  const host=(location.hostname==='192.168.4.1'&&r.ip&&r.ip!=='-')?r.ip:location.hostname;
  const dest=r.https?`https://${host}/`:(host===location.hostname?'/':`http://${host}/`);
  fin.innerHTML=r.https
    ?`<span class="ok">Fertig.</span> Weiter über HTTPS: <a href="${dest}">${dest}</a><br>`+
     `<span class="muted">Der Browser warnt einmal wegen des eigenen Zertifikats; bestätigen und mit demselben Zugang anmelden.</span>`
    :'<span class="ok">Fertig — weiter zum Live-Dashboard…</span>';
  setTimeout(()=>location.href=dest,r.https?2500:1200);
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
    <a href="/config#akku"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 12h8"/></svg> Akku</a>
    <div class="nav-grp">Ausgänge</div>
    <a href="/config#sunspec"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M5 5h14M5 10h14M5 15h14M5 20h8"/></svg> Consolinno</a>
    <a href="/config#mqtt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 11 12 4l8 7M6 9.5V20h12V9.5M10 20v-5h4v5"/></svg> Home Assistant</a>
    <a href="/config#gateway"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M4 7h16M4 17h16M8 7v10M16 7v10"/><circle cx="12" cy="12" r="2"/></svg> Modbus-Gateway</a>
    <a href="/config#knx"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M3 9h18M3 15h18"/><circle cx="8" cy="9" r="1.6"/><circle cx="16" cy="15" r="1.6"/></svg> KNX</a>
    <div class="nav-grp">Gerät</div>
    <a href="/config#features"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="4" width="6" height="6" rx="1.5"/><rect x="14" y="4" width="6" height="6" rx="1.5"/><rect x="4" y="14" width="6" height="6" rx="1.5"/><path d="M17 14v6M14 17h6"/></svg> Funktionen</a>
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
// Menüpunkte nicht gewählter Funktionen ausblenden (Einstellungen > Funktionen schaltet sie wieder ein)
function navFeat(F,disp){
  F=F||{};const off={sunspec:F.sunspec===false,mqtt:F.mqtt===false,gateway:F.gateway===false,knx:F.knx===false,
    tasmota:F.tasmota===false,cloud:F.cloud===false,akku:F.akku===false,display:disp===false};
  document.querySelectorAll('.side nav a').forEach(a=>{const m=/#(\w+)$/.exec(a.getAttribute('href')||'');
    if(m&&m[1] in off)a.classList.toggle('hide',off[m[1]]);});
}
function t(k){return (I18N[LANG]&&I18N[LANG][k])||k;}
function applyThemeLang(s){
  LANG=(s.uiLang==='en')?'en':'de';
  document.documentElement.lang=LANG;
  document.documentElement.dataset.theme=s.uiTheme||'mint';
  const sl=document.getElementById('uiLang'); if(sl)sl.value=LANG;
  const st=document.getElementById('uiTheme'); if(st)st.value=s.uiTheme||'mint';
  const sv=document.getElementById('sideVer'); if(sv)sv.textContent='v'+(s.version||'')+'\n'+(s.ipShown||'');
  if(!window.NAV_FROM_FORM)navFeat(s.features,s.hasDisplay);
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
    ${d.lastNote?('<br>Zuletzt: '+esc(d.lastNote)+' (Addr '+d.lastAddr+' = '+d.lastVal+')'):'<br>Noch kein Write empfangen — in Consolinno §14a/LPP testen.'}</div>
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

// Basic-Auth schickt der Browser auch bei Anfragen fremder Seiten mit (CSRF). Ändernde Anfragen
// deshalb nur von der eigenen Oberfläche annehmen. Werkzeuge ohne Origin/Sec-Fetch-Site (curl,
// Python) bleiben erlaubt. Über den HTTPS-Proxy (127.0.0.1) kommt der Host als X-Forwarded-Host.
bool crossSite(AsyncWebServerRequest *req) {
  const auto m = req->method();
  if (m == HTTP_GET || m == HTTP_HEAD || m == HTTP_OPTIONS) return false;
  if (req->hasHeader("Sec-Fetch-Site")) {
    const String &s = req->header("Sec-Fetch-Site");
    return !(s.equalsIgnoreCase("same-origin") || s.equalsIgnoreCase("none"));
  }
  if (!req->hasHeader("Origin")) return false;
  String origin = req->header("Origin");
  const int p = origin.indexOf("://");
  if (p >= 0) origin = origin.substring(p + 3);
  String host = req->host();
  const bool local = req->client() && req->client()->remoteIP() == IPAddress(127, 0, 0, 1);
  if (local && req->hasHeader("X-Forwarded-Host")) host = req->header("X-Forwarded-Host");
  return !origin.equalsIgnoreCase(host);
}

bool requireAuth(AsyncWebServerRequest *req) {
  if (crossSite(req)) {
    req->send(403, "application/json", "{\"ok\":false,\"error\":\"cross-site request\"}");
    return false;
  }
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
    // Anmeldung prüfen, bevor Speicher reserviert wird (Header sind schon da)
    if (!requireAuth(req)) return nullptr;
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

// Netzwerk neu starten oder den HIT abfragen dauert Sekunden und kann die Verbindung kappen, über die
// gespeichert wurde. Deshalb erst antworten, dann in der Hauptschleife übernehmen.
volatile uint8_t applyPending = 0; // 1 = übernehmen, 2 = übernehmen und HIT sofort abfragen
volatile uint32_t applyAt = 0;
// Langsame Arbeit nie im async_tcp-Task (SPI/HTTP/MQTT/HIT)
volatile uint8_t workFlags = 0;
constexpr uint8_t W_POLL = 1, W_CTRL = 2, W_NEWCODE = 4, W_PROBE = 8;
void addWork(uint8_t w) { __atomic_fetch_or(&workFlags, w, __ATOMIC_SEQ_CST); }
volatile bool ctrlDone = false, ctrlOk = false;
char ctrlErr[96] = "";
AppConfig probeReq;
volatile bool probePending = false, probeDone = false, probeOk = false;
char probeSummary[192] = "", probeRaw[2048] = "";

void scheduleApply(bool pollHit) {
  applyAt = millis() + 300;
  applyPending = pollHit ? 2 : (applyPending ? applyPending : 1);
}

void runPendingApply() {
  if (!applyPending || int32_t(millis() - applyAt) < 0) return;
  const bool poll = applyPending == 2;
  applyPending = 0;
  auto &c = ConfigStore::get();
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
  if (poll) HitClient::pollNow();
  SunSpecServer::rebuildMap();
}

void runDeferredWork() {
  // Erst die gerade gespeicherten Einstellungen übernehmen, sonst pollt/schreibt man mit den alten
  if (applyPending) return;
  // Web-Task setzt Bits auf dem anderen Kern: atomar abholen, sonst geht ein Auftrag verloren
  const uint8_t f = __atomic_exchange_n(&workFlags, uint8_t(0), __ATOMIC_SEQ_CST);
  if (!f) return;
  if (f & W_NEWCODE) {
    auto &c = ConfigStore::get();
    NetBoot::applyConfig(c);
    StatusDisplay::applyConfig(c);
  }
  if (f & W_POLL) {
    HitClient::pollNow();
    DataBroker::reconcile();
    TasmotaClient::pollNow();
    SunSpecServer::rebuildMap();
    MqttHa::publishNow();
  }
  if (f & W_CTRL) {
    ctrlOk = HitControl::applyAll();
    strlcpy(ctrlErr, HitControl::lastError().c_str(), sizeof(ctrlErr));
    ctrlDone = true;
  }
  if (f & W_PROBE) {
    auto &c = ConfigStore::get();
    TasmotaClient::applyConfig(probeReq);
    String raw, summary;
    probeOk = TasmotaClient::probe(raw, summary);
    strlcpy(probeSummary, summary.c_str(), sizeof(probeSummary));
    strlcpy(probeRaw, raw.c_str(), sizeof(probeRaw));
    TasmotaClient::applyConfig(c);
    if (probeOk) {
      c.tasmotaEnabled = true;
      strlcpy(c.tasmotaHost, probeReq.tasmotaHost, sizeof(c.tasmotaHost));
      c.tasmotaPort = probeReq.tasmotaPort;
      strlcpy(c.tasmotaUser, probeReq.tasmotaUser, sizeof(c.tasmotaUser));
      if (probeReq.tasmotaPass[0])
        strlcpy(c.tasmotaPass, probeReq.tasmotaPass, sizeof(c.tasmotaPass));
      ConfigStore::save();
      TasmotaClient::applyConfig(c);
      SunSpecServer::rebuildMap();
    }
    probeDone = true;
    probePending = false;
  }
}

void jsonEscapeAppend(String &out, const char *s) {
  for (; s && *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if (uint8_t(*s) >= 0x20) out += *s;
  }
}

// Leere Formularfelder kommen als 0: dann den alten Wert behalten
uint16_t port(JsonVariantConst v, uint16_t old) {
  const long p = v.as<long>();
  return (p >= 1 && p <= 65535) ? uint16_t(p) : old;
}
uint8_t unitId(JsonVariantConst v, uint8_t old) {
  const long u = v.as<long>();
  return (u >= 1 && u <= 247) ? uint8_t(u) : old;
}

void applyJson(JsonObjectConst o, AppConfig &c) {
  if (!o["useEthernet"].isNull()) c.useEthernet = o["useEthernet"];
  if (!o["wifiEnabled"].isNull()) c.wifiEnabled = o["wifiEnabled"];
  if (!o["wifiApMode"].isNull()) c.wifiApMode = o["wifiApMode"];
  if (!o["setupComplete"].isNull()) c.setupComplete = o["setupComplete"];
  // leerer Name würde das Geräte-WLAN abschalten (softAP lehnt ihn ab)
  if (o["wifiSsid"].is<const char *>() && o["wifiSsid"].as<const char *>()[0])
    strlcpy(c.wifiSsid, o["wifiSsid"], sizeof(c.wifiSsid));
  if (o["staSsid"].is<const char *>()) strlcpy(c.staSsid, o["staSsid"], sizeof(c.staSsid));
  // Passwort wird nie ausgeliefert; leeres Feld heißt unverändert
  if (o["staPass"].is<const char *>() && o["staPass"].as<const char *>()[0])
    strlcpy(c.staPass, o["staPass"], sizeof(c.staPass));
  if (o["staSsid"].is<const char *>() && !c.staSsid[0]) c.staPass[0] = '\0';
  // wifiPass / displayCode: random — nicht aus Form überschreiben außer explizit
  if (o["webUser"].is<const char *>()) strlcpy(c.webUser, o["webUser"], sizeof(c.webUser));
  // Passwörter nie ausliefern; leeres Feld = unverändert
  if (o["webPass"].is<const char *>() && o["webPass"].as<const char *>()[0])
    strlcpy(c.webPass, o["webPass"], sizeof(c.webPass));
  if (!o["webPassClear"].isNull() && o["webPassClear"].as<bool>()) c.webPass[0] = '\0';
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
  if (!o["inverterType"].isNull()) {
    const int t = o["inverterType"].as<int>();
    if (t >= 0 && t < INVERTER_TYPE_COUNT) c.inverterType = static_cast<InverterType>(t);
  }
  if (!o["hitMode"].isNull())
    c.hitMode = o["hitMode"].as<int>() == 1 ? HitLinkMode::RtuRs485 : HitLinkMode::Tcp;
  if (o["hitHost"].is<const char *>()) strlcpy(c.hitHost, o["hitHost"], sizeof(c.hitHost));
  if (!o["hitPort"].isNull()) c.hitPort = port(o["hitPort"], c.hitPort);
  if (!o["hitUnitId"].isNull()) c.hitUnitId = unitId(o["hitUnitId"], c.hitUnitId);
  if (!o["hitPollMs"].isNull()) c.hitPollMs = o["hitPollMs"];
  if (!o["hitRtuBaud"].isNull()) c.hitRtuBaud = o["hitRtuBaud"];
  if (!o["demoMode"].isNull()) c.demoMode = o["demoMode"];
  if (!o["sunspecEnabled"].isNull()) c.sunspecEnabled = o["sunspecEnabled"];
  if (!o["sunspecPort"].isNull()) c.sunspecPort = port(o["sunspecPort"], c.sunspecPort);
  if (!o["sunspecUnitId"].isNull()) c.sunspecUnitId = unitId(o["sunspecUnitId"], c.sunspecUnitId);
  if (!o["invertPowerSign"].isNull()) c.invertPowerSign = o["invertPowerSign"];
  if (!o["gatewayEnabled"].isNull()) c.gatewayEnabled = o["gatewayEnabled"];
  if (!o["gatewayPort"].isNull()) c.gatewayPort = port(o["gatewayPort"], c.gatewayPort);
  if (!o["displayEnabled"].isNull()) c.displayEnabled = o["displayEnabled"];
  if (!o["displayBrightness"].isNull()) c.displayBrightness = constrain(o["displayBrightness"].as<int>(), 0, 255);
  if (!o["displayAutoPage"].isNull()) c.displayAutoPage = o["displayAutoPage"];
  if (!o["displayPageS"].isNull()) c.displayPageMs = constrain(o["displayPageS"].as<uint32_t>(), 3u, 600u) * 1000;
  if (!o["displayTimeoutS"].isNull()) c.displayTimeoutS = constrain(o["displayTimeoutS"].as<uint32_t>(), 0u, 36000u);
  if (!o["tasmotaEnabled"].isNull()) c.tasmotaEnabled = o["tasmotaEnabled"];
  if (!o["tasmotaLiveGrid"].isNull()) c.tasmotaLiveGrid = o["tasmotaLiveGrid"];
  if (!o["tasmotaMeterSpoof"].isNull()) c.tasmotaMeterSpoof = o["tasmotaMeterSpoof"];
  if (o["tasmotaHost"].is<const char *>()) strlcpy(c.tasmotaHost, o["tasmotaHost"], sizeof(c.tasmotaHost));
  if (!o["tasmotaPort"].isNull()) c.tasmotaPort = port(o["tasmotaPort"], c.tasmotaPort);
  if (!o["tasmotaPollMs"].isNull()) c.tasmotaPollMs = o["tasmotaPollMs"];
  if (o["tasmotaUser"].is<const char *>()) strlcpy(c.tasmotaUser, o["tasmotaUser"], sizeof(c.tasmotaUser));
  if (o["tasmotaPass"].is<const char *>() && o["tasmotaPass"].as<const char *>()[0])
    strlcpy(c.tasmotaPass, o["tasmotaPass"], sizeof(c.tasmotaPass));
  if (!o["mqttEnabled"].isNull()) c.mqttEnabled = o["mqttEnabled"];
  if (!o["mqttDiscovery"].isNull()) c.mqttDiscovery = o["mqttDiscovery"];
  if (o["mqttHost"].is<const char *>()) strlcpy(c.mqttHost, o["mqttHost"], sizeof(c.mqttHost));
  if (!o["mqttPort"].isNull()) c.mqttPort = port(o["mqttPort"], c.mqttPort);
  if (o["mqttUser"].is<const char *>()) strlcpy(c.mqttUser, o["mqttUser"], sizeof(c.mqttUser));
  if (o["mqttPass"].is<const char *>() && o["mqttPass"].as<const char *>()[0])
    strlcpy(c.mqttPass, o["mqttPass"], sizeof(c.mqttPass));
  if (o["mqttBase"].is<const char *>()) strlcpy(c.mqttBase, o["mqttBase"], sizeof(c.mqttBase));
  if (!o["mqttPublishMs"].isNull()) c.mqttPublishMs = o["mqttPublishMs"];
  if (!o["mqttIngestEnabled"].isNull()) c.mqttIngestEnabled = o["mqttIngestEnabled"];
  if (o["mqttIngestTopic"].is<const char *>()) strlcpy(c.mqttIngestTopic, o["mqttIngestTopic"], sizeof(c.mqttIngestTopic));
  if (!o["mqttIngestMaxAgeMs"].isNull()) c.mqttIngestMaxAgeMs = o["mqttIngestMaxAgeMs"];
  if (!o["cloudEnabled"].isNull()) c.cloudEnabled = o["cloudEnabled"];
  if (o["cloudHost"].is<const char *>()) strlcpy(c.cloudHost, o["cloudHost"], sizeof(c.cloudHost));
  if (o["cloudUser"].is<const char *>()) strlcpy(c.cloudUser, o["cloudUser"], sizeof(c.cloudUser));
  if (o["cloudPass"].is<const char *>() && o["cloudPass"].as<const char *>()[0])
    strlcpy(c.cloudPass, o["cloudPass"], sizeof(c.cloudPass));
  if (o["cloudStationId"].is<const char *>()) strlcpy(c.cloudStationId, o["cloudStationId"], sizeof(c.cloudStationId));
  if (!o["cloudPollMs"].isNull()) c.cloudPollMs = o["cloudPollMs"];
  if (!o["cloudMaxAgeMs"].isNull()) c.cloudMaxAgeMs = o["cloudMaxAgeMs"];
  if (!o["batEnabled"].isNull()) c.batEnabled = o["batEnabled"];
  if (!o["batHitEnabled"].isNull()) c.batHitEnabled = o["batHitEnabled"];
  if (!o["batHitPollS"].isNull()) c.batHitPollS = constrain(o["batHitPollS"].as<long>(), 30L, 3600L);
  if (!o["batCloud"].isNull()) {
    const int t = o["batCloud"].as<int>();
    if (t >= 0 && t < BATTERY_CLOUD_COUNT) c.batCloud = static_cast<BatteryCloudType>(t);
  }
  if (o["batCloudHost"].is<const char *>() && o["batCloudHost"].as<const char *>()[0])
    strlcpy(c.batCloudHost, o["batCloudHost"], sizeof(c.batCloudHost));
  if (o["batCloudAppId"].is<const char *>()) strlcpy(c.batCloudAppId, o["batCloudAppId"], sizeof(c.batCloudAppId));
  if (o["batCloudSecret"].is<const char *>() && o["batCloudSecret"].as<const char *>()[0])
    strlcpy(c.batCloudSecret, o["batCloudSecret"], sizeof(c.batCloudSecret));
  if (o["batCloudSn"].is<const char *>()) strlcpy(c.batCloudSn, o["batCloudSn"], sizeof(c.batCloudSn));
  if (!o["batCloudPollS"].isNull())
    c.batCloudPollS = constrain(o["batCloudPollS"].as<long>(), long(BAT_CLOUD_MIN_S), 3600L);
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
    // Bis die Einrichtung fertig ist, führt jede Seite dorthin; Schnittstellen, CSS und Schriften braucht die Einrichtung selbst
    if (!c.setupComplete && req->method() == HTTP_GET) {
      const String &u = req->url();
      if (u != "/setup" && !u.startsWith("/api/") && u != "/a.css" && !u.startsWith("/f/")) {
        req->redirect("/setup");
        return;
      }
    }
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
    out += ",\"lastNote\":\"";
    jsonEscapeAppend(out, DebugLog::lastNote());
    out += "\"";
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
              {
                // Geräte-WLAN geht gleich aus; wer darüber eingerichtet hat, braucht die Adresse im Heimnetz
                const bool eth = gLive.ethOk && strcmp(gLive.ipEth, "-") != 0;
                String out = "{\"ok\":true,\"https\":";
                out += (c.httpsEnabled && HttpsServer::running()) ? "true" : "false";
                out += ",\"ip\":\"";
                out += eth ? gLive.ipEth : gLive.ipShown;
                out += "\"}";
                req->send(200, "application/json", out);
              }
              scheduleApply(true);
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
    doc["hitPollsDone"] = gLive.hitPollsDone;
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
    doc["tasmotaLiveGrid"] = c.tasmotaLiveGrid;
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
    {
      // gewählte Funktionen: die Oberfläche blendet Menüpunkte nicht genutzter Dienste aus
      JsonObject f = doc["features"].to<JsonObject>();
      f["sunspec"] = c.sunspecEnabled;
      f["mqtt"] = c.mqttEnabled || c.mqttIngestEnabled;
      f["gateway"] = c.gatewayEnabled;
      f["knx"] = c.knxEnabled;
      f["tasmota"] = c.tasmotaEnabled;
      f["cloud"] = c.cloudEnabled;
      f["akku"] = c.batEnabled;
    }
    doc["board"] = BOARD_NAME;
    doc["hasDisplay"] = BOARD_HAS_DISPLAY;
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
    doc["webPass"] = "";
    doc["webPassSet"] = c.webPass[0] != 0;
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
    doc["inverterType"] = int(c.inverterType);
    doc["hitHost"] = c.hitHost;
    doc["hitPort"] = c.hitPort;
    doc["hitUnitId"] = c.hitUnitId;
    doc["hitPollMs"] = c.hitPollMs;
    doc["hitRtuBaud"] = c.hitRtuBaud;
    doc["demoMode"] = c.demoMode;
    doc["sunspecEnabled"] = c.sunspecEnabled;
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
    doc["tasmotaLiveGrid"] = c.tasmotaLiveGrid;
    doc["tasmotaMeterSpoof"] = c.tasmotaMeterSpoof;
    doc["tasmotaHost"] = c.tasmotaHost;
    doc["tasmotaPort"] = c.tasmotaPort;
    doc["tasmotaPollMs"] = c.tasmotaPollMs;
    doc["tasmotaUser"] = c.tasmotaUser;
    doc["tasmotaPass"] = "";
    doc["tasmotaPassSet"] = c.tasmotaPass[0] != 0;
    doc["mqttEnabled"] = c.mqttEnabled;
    doc["mqttDiscovery"] = c.mqttDiscovery;
    doc["mqttHost"] = c.mqttHost;
    doc["mqttPort"] = c.mqttPort;
    doc["mqttUser"] = c.mqttUser;
    doc["mqttPass"] = "";
    doc["mqttPassSet"] = c.mqttPass[0] != 0;
    doc["mqttBase"] = c.mqttBase;
    doc["mqttPublishMs"] = c.mqttPublishMs;
    doc["mqttIngestEnabled"] = c.mqttIngestEnabled;
    doc["mqttIngestTopic"] = c.mqttIngestTopic;
    doc["mqttIngestMaxAgeMs"] = c.mqttIngestMaxAgeMs;
    doc["cloudEnabled"] = c.cloudEnabled;
    doc["cloudHost"] = c.cloudHost;
    doc["cloudUser"] = c.cloudUser;
    doc["cloudPass"] = "";
    doc["cloudPassSet"] = c.cloudPass[0] != 0;
    doc["cloudStationId"] = c.cloudStationId;
    doc["cloudPollMs"] = c.cloudPollMs;
    doc["cloudMaxAgeMs"] = c.cloudMaxAgeMs;
    doc["batEnabled"] = c.batEnabled;
    doc["batHitEnabled"] = c.batHitEnabled;
    doc["batHitPollS"] = c.batHitPollS;
    doc["batCloud"] = int(c.batCloud);
    doc["batCloudHost"] = c.batCloudHost;
    doc["batCloudAppId"] = c.batCloudAppId;
    doc["batCloudSecret"] = "";
    doc["batCloudSecretSet"] = c.batCloudSecret[0] != 0;
    doc["batCloudSn"] = c.batCloudSn;
    doc["batCloudPollS"] = c.batCloudPollS;
    doc["batCloudMinS"] = BAT_CLOUD_MIN_S;
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
              req->send(200, "application/json", "{\"ok\":true}");
              scheduleApply(false);
            });

  server.on("/api/control/apply", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    ctrlDone = false;
    ctrlOk = false;
    ctrlErr[0] = '\0';
    addWork(W_CTRL);
    req->send(200, "application/json", "{\"ok\":true,\"pending\":true,\"done\":false}");
  });

  server.on("/api/control/result", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument out;
    out["pending"] = !ctrlDone;
    out["done"] = ctrlDone;
    out["ok"] = ctrlDone && ctrlOk;
    out["error"] = ctrlErr;
    String s; serializeJson(out, s);
    req->send(200, "application/json", s);
  });

  server.on("/api/battery", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    BatteryData::toJson(doc, ConfigStore::get());
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
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
    addWork(W_NEWCODE); // NetBoot + Display in der Hauptschleife (SPI!)
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/display/tap", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    StatusDisplay::requestTap();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/poll", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    addWork(W_POLL);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/tasmota/probe", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              if (probePending) {
                req->send(409, "application/json", "{\"ok\":false,\"pending\":true,\"summary\":\"Probe läuft bereits\"}");
                return;
              }
              JsonDocument in;
              deserializeJson(in, body);
              auto &c = ConfigStore::get();
              probeReq = c;
              if (in["tasmotaHost"].is<const char *>())
                strlcpy(probeReq.tasmotaHost, in["tasmotaHost"], sizeof(probeReq.tasmotaHost));
              if (!in["tasmotaPort"].isNull()) probeReq.tasmotaPort = in["tasmotaPort"];
              if (in["tasmotaUser"].is<const char *>())
                strlcpy(probeReq.tasmotaUser, in["tasmotaUser"], sizeof(probeReq.tasmotaUser));
              if (in["tasmotaPass"].is<const char *>() && in["tasmotaPass"].as<const char *>()[0])
                strlcpy(probeReq.tasmotaPass, in["tasmotaPass"], sizeof(probeReq.tasmotaPass));
              probeReq.tasmotaEnabled = true;
              probeDone = false;
              probeOk = false;
              probeSummary[0] = probeRaw[0] = '\0';
              probePending = true;
              addWork(W_PROBE);
              req->send(200, "application/json", "{\"ok\":true,\"pending\":true}");
            });

  server.on("/api/tasmota/probe/result", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument out;
    out["pending"] = probePending || !probeDone;
    out["ok"] = probeOk;
    out["summary"] = probeSummary;
    out["raw"] = probeRaw;
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
  runPendingApply();
  runDeferredWork();
  OtaUpdate::loop();
  HttpsServer::loop();
}

} // namespace WebConfig
