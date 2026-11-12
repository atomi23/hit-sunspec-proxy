#!/usr/bin/env python3
"""Extract real WebConfig HTML/CSS into static preview pages with mocked APIs."""
import json
import re
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = (ROOT / "src" / "WebConfig.cpp").read_text(encoding="utf-8")
OUT = pathlib.Path(__file__).resolve().parent


def extract(name: str) -> str:
    m = re.search(
        rf'const char {name}\[\] PROGMEM = R"(\w+)\((.*?)\)\1";',
        SRC,
        re.S,
    )
    if not m:
        raise SystemExit(f"missing {name}")
    return m.group(2)


css = extract("UI_CSS")
dash = extract("DASH_HTML")
idx = extract("INDEX_HTML")
(OUT / "a.css").write_text(css, encoding="utf-8")

mock_status = {
    "version": "0.4.2",
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

dash_js = f"""
<script>
const MOCK={json.dumps(mock_status)};
MOCK.uiTheme=new URLSearchParams(location.search).get('theme')||'mint';
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/status')) return {{ok:true,json:async()=>MOCK}};
  if(u.includes('/api/poll')) return {{ok:true,json:async()=>({{ok:true}})}};
  return _f(u,o);
}};
</script>
"""
(OUT / "index.html").write_text(dash.replace("<body>", "<body>" + dash_js, 1), encoding="utf-8")

kp = (ROOT / "include" / "KnxPoints.h").read_text(encoding="utf-8")
meta = []
for m in re.finditer(
    r'\{\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*KnxValKind::(\w+)\s*\}',
    kp,
):
    meta.append(
        {"id": m.group(1), "label": m.group(2), "dpt": int(m.group(3)), "kind": m.group(4)}
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
    "hitPollMs": 25000,
    "wifiSsid": "HIT-Proxy",
    "wifiEnabled": True,
    "useEthernet": True,
    "sunspecPort": 1502,
    "sunspecUnitId": 1,
    "displayEnabled": True,
    "displayBrightness": 80,
    "displayPageS": 8,
    "displayTimeoutS": 300,
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
}

cfg_js = f"""
<script>
const META={json.dumps({"points": meta})};
const CFG={json.dumps(cfg)};
const STATUS={json.dumps(mock_status)};
const TLS={{subject:"CN=HIT-A1B2, O=HIT-SunSpec-Proxy",issuer:"CN=HIT-A1B2, O=HIT-SunSpec-Proxy",validTo:"2029-02-08",
  sha256:"3F:9A:0C:51:7E:D2:88:14:B6:29:4D:E0:73:1F:C5:A8:62:0B:9E:47:D3:15:8C:F1:2A:6E:B0:94:57:C3:0D:E9",
  chain:1,source:"self",running:true,enabled:true}};
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
(OUT / "config.html").write_text(idx.replace("<body>", "<body>" + cfg_js, 1), encoding="utf-8")

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
const STATUS={json.dumps(mock_status)};
STATUS.uiTheme=new URLSearchParams(location.search).get('theme')||'mint';
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
(OUT / "history.html").write_text(hist.replace("<body>", "<body>" + hist_js, 1), encoding="utf-8")

# Protokoll: erfundene Ereignisse, relative Zeiten ab Seitenaufruf
dbg = extract("DEBUG_HTML")
up = 9 * 3600 * 1000 + 42 * 60 * 1000
ev = [
    (0, "I", "System", "Start v0.4.2, Grund: Software-Neustart (OTA)"),
    (4200, "I", "Cloud", "Daten kommen an"),
    (2 * 3600e3, "W", "HIT", "Abfrage fehlgeschlagen bei Reg 1804: keine Antwort (Timeout)"),
    (2 * 3600e3 + 26e3, "I", "HIT", "wieder erreichbar nach 1 Fehlversuch (25 s)"),
    (5 * 3600e3, "W", "HIT", "Abfrage fehlgeschlagen bei Reg 27: vom HIT getrennt"),
    (5 * 3600e3 + 500e3, "E", "HIT", "seit 500 s keine Daten (20 Versuche): Verbindungsaufbau fehlgeschlagen"),
    (5 * 3600e3 + 640e3, "I", "HIT", "wieder erreichbar nach 26 Fehlversuchen (640 s)"),
    (8 * 3600e3, "W", "Cloud", "Datenabruf fehlgeschlagen (HTTP 502)"),
    (8 * 3600e3 + 61e3, "I", "Cloud", "Daten kommen an"),
]
events = [{"t": int(t), "ts": 0, "lv": lv, "src": s, "msg": m} for t, lv, s, m in ev]
dbg_js = f"""
<script>
const STATUS={json.dumps(mock_status)};
STATUS.uiTheme=new URLSearchParams(location.search).get('theme')||'mint';
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
(OUT / "debug.html").write_text(dbg.replace("<body>", "<body>" + dbg_js, 1), encoding="utf-8")
print(f"wrote {len(meta)} knx points -> {OUT}")
