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
    "version": "0.4.0",
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
const _f=window.fetch;
window.fetch=async(u,o)=>{{
  u=String(u);
  if(u.includes('/api/knx/meta')) return {{ok:true,json:async()=>META}};
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
print(f"wrote {len(meta)} knx points -> {OUT}")
