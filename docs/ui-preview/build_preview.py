#!/usr/bin/env python3
"""Extract real WebConfig HTML/CSS into static preview pages with mocked APIs."""
import json
import re
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = (ROOT / "src" / "WebConfig.cpp").read_text(encoding="utf-8")
OUT = pathlib.Path(__file__).resolve().parent
VERSION = re.search(r'PROXY_VERSION=\\"([^"]+)\\"', (ROOT / "platformio.ini").read_text()).group(1)


def extract(name: str) -> str:
    m = re.search(
        rf'const char {name}\[\] PROGMEM = R"(\w+)\((.*?)\)\1";',
        SRC,
        re.S,
    )
    if not m:
        raise SystemExit(f"missing {name}")
    return m.group(2)


routes = {
    "/": "index.html",
    **{f"/{name}": f"{name}.html" for name in ("config", "setup", "history", "debug", "bluetooth", "system")},
    "/a.css": "a.css",
    "/b.js": "b.js",
    "/f/barlow-600.woff": "f/barlow-600.woff",
}


def static_url(match):
    path = match[2]
    return match[1] + routes.get(path, path)


def write_page(name, html, mock_js):
    html = re.sub(r'^[ \t]*<a href="/ota">[^\n]*\n', "", html, flags=re.M)
    html = re.sub(r"""((?:href|src)=["'])(/[^"'#]*)""", static_url, html)
    html = re.sub(r"(location\.href=')(/[^']*)", static_url, html)
    (OUT / routes[name]).write_text(html.replace("<body>", "<body>" + mock_js + preview_js, 1), encoding="utf-8")


preview_js = r"""
<script>
const previewParams=new URLSearchParams(location.search);
function previewLink(path){
  const url=new URL(path,location.href);
  for(const key of ['lang','theme'])if(previewParams.has(key))url.searchParams.set(key,previewParams.get(key));
  return url.href;
}
document.addEventListener('click',e=>{
  const a=e.target.closest('a[href]');
  if(a&&a.getAttribute('href').match(/^\w+\.html(?:#|$)/)&&!e.ctrlKey&&!e.metaKey&&!e.shiftKey&&!e.altKey){
    e.preventDefault();
    location.href=previewLink(a.href);
  }
});
const previewFetch=window.fetch;
window.fetch=async(u,o)=>{
  if(u==='/api/config'&&o?.method==='POST'&&typeof o.body==='string'){
    const prefs=JSON.parse(o.body);
    if('uiLang' in prefs&&('uiTheme' in prefs||prefs.setupComplete===false)){
      previewParams.set('lang',prefs.uiLang);
      if('uiTheme' in prefs)previewParams.set('theme',prefs.uiTheme);
      history.replaceState(null,'',location.pathname+'?'+previewParams+location.hash);
      return {ok:true};
    }
  }
  return previewFetch(u,o);
};
</script>
"""


css = extract("UI_CSS").replace("url(/f/", "url(f/")
dash = extract("DASH_HTML")
idx = extract("INDEX_HTML")
(OUT / "a.css").write_text(css, encoding="utf-8")
ui_js = re.sub(r"""(href=["'])(/[^"'#]*)""", static_url, extract("UI_JS")).replace(
    "location.pathname==='/bluetooth'", "location.pathname.endsWith('/bluetooth.html')"
).replace("getAttribute('href')!=='/debug'", "getAttribute('href')!=='debug.html'")
(OUT / "b.js").write_text(ui_js, encoding="utf-8")

mock_status = {
    "version": VERSION,
    "setupComplete": True,
    "demoMode": True,
    "ipShown": "192.0.2.80",
    "ipEth": "192.0.2.80",
    "ipWifi": "192.0.2.81",
    "hitOk": True,
    "sunspecClient": True,
    "mqttOk": False,
    "mqttIngestOk": False,
    "cloudOk": False,
    "knxOk": True,
    "ethOk": True,
    "wifiOk": True,
    "dataOrigin": "NATIVE",
    "pvW": 4820,
    "pv1W": 1210,
    "pv2W": 1180,
    "pv3W": 1240,
    "pv4W": 1190,
    "batW": -620,
    "batV": 51.2,
    "batA": -12.1,
    "soc": 74,
    "soh": 98,
    "gridW": 210,
    "gridW1": 70,
    "gridW2": 68,
    "gridW3": 72,
    "v1": 231.2,
    "v2": 230.8,
    "v3": 231.5,
    "i1": 1.1,
    "i2": 1.0,
    "i3": 1.2,
    "freqHz": 50.01,
    "energyPvKwh": 12840.5,
    "energyImpKwh": 210.2,
    "energyExpKwh": 8420.1,
    "tasmotaOk": False,
    "tasmotaPowerW": 0,
    "tasmotaTotalKwh": 0,
    "tasmotaExportKwh": 0,
    "tasmotaSensorKey": "",
    "wMaxLimPct": 10000,
    "wMaxLimEna": False,
    "lastWriteNote": "",
    "lastHitAgeMs": 1200,
    "status": "ok",
}
mock_status.update({
    "board": "T-Connect-Pro",
    "hasDisplay": True,
    "hasBleLed": True,
    "features": {"sunspec": True, "mqtt": False, "gateway": True, "knx": True, "tasmota": False, "cloud": False},
})

mock_ble = {
    "supported": True,
    "enabled": True,
    "state": "bereit",
    "connected": False,
    "name": "ELK-BLEDOM-DEMO",
    "addr": "02:00:00:00:00:01",
    "on": True,
    "bri": 65,
    "rgb": "#ffffff",
    "found": [],
}

def status_js(name):
    return (f"const {name}={json.dumps(mock_status)};\n"
            f"{name}.uiLang=new URLSearchParams(location.search).get('lang')==='en'?'en':'de';\n"
            f"{name}.uiTheme=new URLSearchParams(location.search).get('theme')||'mint';")


dash_js = f"""
<script>
{status_js("MOCK")}
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/status')) return {{ok:true,json:async()=>MOCK}};
  if(u.includes('/api/poll')) return {{ok:true,json:async()=>({{ok:true}})}};
  return _f(u,o);
}};
</script>
"""
write_page("/", dash, dash_js)

kp = (ROOT / "include" / "KnxPoints.h").read_text(encoding="utf-8")
meta = []
for m in re.finditer(
    r'\{\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*KnxValKind::(\w+)\s*\}',
    kp,
):
    meta.append(
        {"id": m.group(1), "label": m.group(2), "labelEn": m.group(3), "dpt": int(m.group(4)), "kind": m.group(5)}
    )

ch = []
enabled = {0, 1, 2, 13, 16, 18, 22, 28}  # sample points with GA filled
for i, point in enumerate(meta):
    on = i in enabled
    ch.append(
        {
            "ena": on,
            "ga": f"1/2/{10 + i}" if on else "",
            "dpt": point["dpt"],
        }
    )

cfg = {
    "knxEnabled": True,
    "knxPa": "1.1.240",
    "knxPublishMs": 10000,
    "knxOnlyOnChange": True,
    "knxMinDeltaW": 50,
    "knxCh": ch,
    "deviceName": "HIT-SunSpec-Proxy",
    "demoMode": True,
    "hitHost": "192.0.2.50",
    "hitPort": 502,
    "hitUnitId": 1,
    "hitPollMs": 30000,
    "wifiSsid": "HIT-Proxy",
    "wifiEnabled": True,
    "useEthernet": True,
    "sunspecPort": 1502,
    "sunspecUnitId": 1,
    "displayEnabled": True,
    "displayBrightness": 80,
    "displayPageS": 8,
    "displayTimeoutS": 300,
    "displayScheduleEnabled": True,
    "displayWeek": [{"enabled": day < 5, "start": 420, "end": 1320} for day in range(7)],
    "bleLedEnabled": mock_ble["enabled"],
    "bleLedName": mock_ble["name"],
    "bleLedAddr": mock_ble["addr"],
    "gatewayEnabled": True,
    "gatewayPort": 502,
    "mqttEnabled": False,
    "cloudEnabled": False,
    "tasmotaEnabled": False,
    "hitWriteEnabled": False,
    "setupComplete": True,
    "webUser": "admin",
    "httpsEnabled": True,
    "httpsRedirect": True,
    "wifiApMode": False,
    "invertPowerSign": False,
    "hitMode": 0,
    "hitRtuBaud": 9600,
    "mqttDiscovery": True,
    "mqttHost": "",
    "mqttPort": 1883,
    "mqttUser": "",
    "mqttPass": "",
    "mqttBase": "hitproxy",
    "mqttPublishMs": 15000,
    "mqttIngestEnabled": False,
    "mqttIngestTopic": "",
    "mqttIngestMaxAgeMs": 120000,
    "cloudHost": "",
    "cloudUser": "",
    "cloudPass": "",
    "cloudStationId": "",
    "cloudPollMs": 60000,
    "cloudMaxAgeMs": 180000,
    "tasmotaMeterSpoof": False,
    "tasmotaHost": "",
    "tasmotaPort": 80,
    "tasmotaPollMs": 5000,
    "tasmotaUser": "",
    "tasmotaPass": "",
    "batMaxChargePct": 100,
    "batMaxDischargePct": 100,
    "batMaxSoc": 100,
    "batMinSoc": 10,
    "apcPwrLmtPct10": 1000,
    "apcEnabled": False,
    "sunspecWriteToHit": False,
    "sunspecEnabled": True,
    "inverterType": 0,
}

cfg_js = f"""
<script>
const META={json.dumps({"points": meta})};
const CFG={json.dumps(cfg)};
{status_js("STATUS")}
const TLS={{subject:"CN=HIT-A1B2, O=HIT-SunSpec-Proxy",issuer:"CN=HIT-A1B2, O=HIT-SunSpec-Proxy",validTo:"2029-02-08",
  sha256:"3F:9A:0C:51:7E:D2:88:14:B6:29:4D:E0:73:1F:C5:A8:62:0B:9E:47:D3:15:8C:F1:2A:6E:B0:94:57:C3:0D:E9",
  chain:1,source:"self",running:true,enabled:true,caSigned:true,caValidTo:"2036-02-08",
  caSha256:"19:5C:89:4A:03:36:5E:B0:A1:C8:17:F9:68:B9:21:54:94:05:BB:41:AA:6C:71:19:90:D2:BF:34:88:E1:B2:E5"}};
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/knx/meta')) return {{ok:true,json:async()=>META}};
  if(u.includes('/api/tls')) return {{ok:true,json:async()=>TLS}};
  if(u.includes('/api/config') && (!o || !o.method || o.method==='GET'))
    return {{ok:true,json:async()=>CFG}};
  if(u.includes('/api/config')) return {{ok:true,json:async()=>({{ok:true}})}};
  if(u.includes('/api/status')) return {{ok:true,json:async()=>STATUS}};
  if(u.includes('/api/poll')) return {{ok:true,json:async()=>({{ok:true}})}};
  return _f(u,o);
}};
window.alert=()=>{{}};
</script>
"""
idx = idx.replace("getAttribute('href')==='/config#", "getAttribute('href')==='config.html#")
write_page("/config", idx, cfg_js)

# Einrichtungsassistent: Erststart, noch nichts eingerichtet
setup = extract("SETUP_HTML").replace(
    ":(host===location.hostname?'/':", ":(host===location.hostname?previewLink('index.html'):"
)
setup_cfg = dict(cfg, setupComplete=False, hitHost="", displayCode="K7Q2-M9XD", knxEnabled=False, gatewayEnabled=False)
setup_js = f"""
<script>
const CFG={json.dumps(setup_cfg)};
{status_js("STATUS")}
CFG.uiLang=STATUS.uiLang;
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/config') && (!o || !o.method || o.method==='GET')) return {{ok:true,json:async()=>CFG}};
  if(u.includes('/api/status')) return {{ok:true,json:async()=>STATUS}};
  return {{ok:true,json:async()=>({{ok:true}})}};
}};
</script>
"""
write_page("/setup", setup, setup_js)

# Verlauf: synthetischer Tag (Sonne, Hausgrundlast, Akku lädt mittags und entlädt abends)
import math
end = 1762905600  # fester Zeitpunkt, Vorschau reproduzierbar
series = {"pv": [], "bat": [], "grid": [], "house": []}
for i in range(288):
    hour = ((end - (287 - i) * 300) / 3600 + 1) % 24
    pv = max(0, 5200 * math.sin(math.pi * (hour - 7) / 10)) if 7 < hour < 17 else 0
    house = 450 + (900 if 6.5 < hour < 8 or 18 < hour < 22 else 0) + 300 * math.sin(i / 7) ** 2
    bat = -min(pv - house, 2500) if pv > house else min(house - pv, 1800) if 17 < hour or hour < 1 else 0
    grid = house - pv - bat
    for k, v in (("pv", pv), ("bat", bat), ("grid", grid), ("house", house)):
        series[k].append(round(v))
day = {"step": 300, "valid": True, "end": end, **series}
days = []
for d in range(1, 32):
    pv = round(4 + 14 * abs(math.sin(d / 5)), 2)
    use = round(9 + 3 * math.cos(d / 3), 2)
    days.append({"k": f"2026-10-{d:02d}" if d <= 31 else "", "pv": pv, "house": use,
                 "imp": round(max(0.4, use - pv * 0.6), 2), "exp": round(max(0, pv * 0.55 - 2), 2),
                 "chg": 4.1, "dis": 3.8})
months = [{"k": f"20{25 + (m // 12)}-{m % 12 + 1:02d}", "pv": round(150 + 450 * math.sin(math.pi * ((m % 12) + 0.5) / 12), 1),
           "house": 310, "imp": round(80 + 120 * math.cos(math.pi * (m % 12) / 6) ** 2, 1),
           "exp": round(40 + 300 * math.sin(math.pi * ((m % 12) + 0.5) / 12), 1), "chg": 90, "dis": 85} for m in range(10, 23)]
years = [{"k": "2025", "pv": 3920.4, "house": 3710, "imp": 980.2, "exp": 1650.3, "chg": 1100, "dis": 1040},
         {"k": "2026", "pv": 4715.2, "house": 3890, "imp": 921.4, "exp": 2240.8, "chg": 1210, "dis": 1150}]
total = {"pv": 8642.9, "imp": 940.7, "exp": 6299.7}
hist = extract("HISTORY_HTML")
hist_js = f"""
<script>
const H={{day:{json.dumps(day)},bars:{{day:{json.dumps(days)},month:{json.dumps(months)},year:{json.dumps(years)}}},total:{json.dumps(total)}}};
{status_js("STATUS")}
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/history/day')) return {{ok:true,json:async()=>H.day}};
  if(u.includes('/api/history/bars')){{const m=u.match(/unit=(\\w+)/),un=m?m[1]:'day';
    return {{ok:true,json:async()=>({{unit:un,bars:H.bars[un]||[],total:H.total,since:20251001}})}};}}
  if(u.includes('/api/status')) return {{ok:true,json:async()=>STATUS}};
  return _f(u,o);
}};
</script>
"""
write_page("/history", hist, hist_js)

# Protokoll: erfundene Ereignisse, relative Zeiten ab Seitenaufruf
dbg = extract("DEBUG_HTML")
up = 9 * 3600 * 1000 + 42 * 60 * 1000
ev = [
    (0, "I", "System", f"Start v{VERSION}, Grund: Neustart per Software"),
    (7000, "I", "HTTPS", "Port 443 aktiv, von Geräte-CA signiertes Zertifikat"),
    (4200, "I", "Cloud", "Daten kommen an"),
    (2 * 3600e3, "W", "HIT", "Abfrage fehlgeschlagen bei Reg 1804: keine Antwort (Timeout)"),
    (2 * 3600e3 + 26e3, "I", "HIT", "wieder erreichbar nach 1 Fehlversuch (25 s)"),
    (5 * 3600e3, "W", "HIT", "Abfrage fehlgeschlagen bei Reg 27: vom HIT getrennt"),
    (5 * 3600e3 + 500e3, "E", "HIT", "seit 500 s keine Daten (20 Versuche): Verbindungsaufbau fehlgeschlagen"),
    (5 * 3600e3 + 640e3, "I", "HIT", "wieder erreichbar nach 26 Fehlversuchen (640 s)"),
    (8 * 3600e3, "W", "Cloud", "Datenabruf fehlgeschlagen (HTTP 502)"),
    (8 * 3600e3 + 61e3, "I", "Cloud", "Daten kommen an"),
    (up - 35000, "I", "LED", "verbunden mit ELK-BLEDOM-DEMO (02:00:00:00:00:01), Schreibkanal fff3"),
    (up - 35000, "I", "LED", "gesendet: ein (7e 04 04 f0 00 01 ff 00 ef)"),
]
events = [{"t": int(t), "ts": 0, "lv": lv, "src": s, "msg": m} for t, lv, s, m in ev]
dbg_js = f"""
<script>
{status_js("STATUS")}
const EV={{uptimeMs:{up},hitPolls:1394,hitFails:27,hitErr:"Verbindungsaufbau fehlgeschlagen",events:{json.dumps(events)}}};
const WR={{uptimeMs:{up},wMaxLimPct:6000,wMaxLimEna:true,sunspecClient:true,hitWriteEnabled:false,sunspecWriteToHit:false,
  count:2,lastAddr:40238,lastVal:6000,lastNote:"WMaxLimPct 60 %",events:[{{t:{up - 300000},fc:16,addr:40238,val:6000,note:"WMaxLimPct 60 %"}},{{t:{up - 299000},fc:6,addr:40242,val:1,note:"WMaxLim_Ena an"}}]}};
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/events')) return {{ok:true,json:async()=>EV}};
  if(u.includes('/api/debug')) return {{ok:true,json:async()=>WR}};
  if(u.includes('/api/status')) return {{ok:true,json:async()=>STATUS}};
  return _f(u,o);
}};
</script>
"""
write_page("/debug", dbg, dbg_js)

ble = extract("BLE_HTML").replace(
    '<div class="ble">',
    '<p class="hint" data-en="Static preview with sample data; no strip is connected.">'
    'Statische Vorschau mit Beispieldaten; kein Strip verbunden.</p><div class="ble">',
    1,
)
ble = re.sub(r'(name="bleLedAddr"[^>]*placeholder=")[^"]*', r'\g<1>02:00:00:00:00:01', ble)
ble_js = f"""
<script>
{status_js("STATUS")}
const BLE={json.dumps(mock_ble)};
const BLE_CFG={{bleLedEnabled:BLE.enabled,bleLedName:BLE.name,bleLedAddr:BLE.addr,bleLedAddrType:0,bleLedVariant:0}};
const EV={{uptimeMs:{up},events:{json.dumps(events)}}};
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u==='/api/status') return {{ok:true,json:async()=>STATUS}};
  if(u==='/api/config'){{
    if(o?.method==='POST'){{
      Object.assign(BLE_CFG,JSON.parse(o.body));
      Object.assign(BLE,{{enabled:BLE_CFG.bleLedEnabled,name:BLE_CFG.bleLedName,addr:BLE_CFG.bleLedAddr}});
      return {{ok:true}};
    }}
    return {{ok:true,json:async()=>BLE_CFG}};
  }}
  if(u==='/api/bleled') return {{ok:true,json:async()=>BLE}};
  if(u==='/api/bleled/scan'){{
    BLE.found=[{{name:'ELK-BLEDOM-DEMO',addr:'02:00:00:00:00:01',rssi:-48,type:0}}];
    return {{ok:true}};
  }}
  if(u.startsWith('/api/bleled/set?')) return {{ok:true}};
  if(u==='/api/events') return {{ok:true,json:async()=>EV}};
  return _f(u,o);
}};
</script>
"""
write_page("/bluetooth", ble, ble_js)

system = extract("SYS_HTML")
mock_system = {
    **{key: mock_status[key] for key in ("board", "hasDisplay", "hasBleLed", "version", "ipEth", "ipWifi", "features")},
    "chip": "ESP32-S3", "chipRev": 2, "cores": 2, "cpuMhz": 240, "cpu": [24, 11], "tasks": 22,
    "heapBytes": 385024, "heapFree": 122880, "heapMin": 98304, "heapBlock": 65536,
    "psramBytes": 8388608, "psramFree": 5242880, "flashBytes": 16777216,
    "fsBytes": 3145728, "fsUsed": 262144, "sketchBytes": 2195456, "sketchFree": 6291456,
    "arduino": "3.3.0", "sdk": "5.5.0", "build": "Nov 20 2026 12:00:00",
    "uptimeS": up // 1000, "mac": "02:00:00:00:00:02", "https": True,
}
system_js = f"""
<script>
{status_js("STATUS")}
const SYSTEM={json.dumps(mock_system)};
SYSTEM.resetReason=STATUS.uiLang==='en'?'Power on':'Einschalten';
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  if(u==='/api/status') return {{ok:true,json:async()=>STATUS}};
  if(u==='/api/system') return {{ok:true,json:async()=>SYSTEM}};
  return _f(u,o);
}};
</script>
"""
write_page("/system", system, system_js)
print(f"wrote {len(meta)} knx points -> {OUT}")
