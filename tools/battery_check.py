#!/usr/bin/env python3
"""Akkudaten prüfen.

    python tools/battery_check.py 192.168.1.50 --user admin --pass CODE
    python tools/battery_check.py 192.168.1.50 --json
    python tools/battery_check.py --sign-selftest

Ohne --sign-selftest: holt /api/battery und prüft den Aufbau (hit, packs, cloudData, merged).
Im Demo-Modus müssen HIT-Werte und ein Pack vorhanden sein.

--sign-selftest: rechnet die Dyness-Signatur nach dem Verfahren der Home-Assistant-Integration
(tools/dyness_test.py) für den festen Testvektor, der auch in src/BatteryDyness.cpp steht.
Die Firmware prüft denselben Vektor beim Start und schreibt "Dyness sign selftest: ok" ins serielle Log.
"""
import argparse
import base64
import hashlib
import hmac
import json
import sys
import urllib.request

VECTOR = {
    "secret": "test-secret",
    "body": {"deviceSn": "X"},
    "date": "Tue, 18 Nov 2026 10:00:00 GMT",
    "path": "/v1/device/realTime/data",
    "md5": "K18hg9F3/G050B4mkxEGfQ==",
    "sig": "+U7BkOuEwKBtyTqqUQB0kBj8Pig=",
}


def dyness_sign(secret, body, date, path):
    md5 = base64.b64encode(hashlib.md5(body.encode()).digest()).decode()
    sts = f"POST\n{md5}\napplication/json\n{date}\n{path}"
    sig = base64.b64encode(hmac.new(secret.encode(), sts.encode(), "sha1").digest()).decode()
    return md5, sig


def selftest():
    body = json.dumps(VECTOR["body"], separators=(",", ":"), sort_keys=True)
    md5, sig = dyness_sign(VECTOR["secret"], body, VECTOR["date"], VECTOR["path"])
    ok = md5 == VECTOR["md5"] and sig == VECTOR["sig"]
    print(f"Body {body}\nContent-MD5 {md5}\nSignatur {sig}\n{'OK' if ok else 'FEHLER'}")
    return ok


def fetch(host, user, pw):
    req = urllib.request.Request(f"http://{host}/api/battery")
    if pw:
        tok = base64.b64encode(f"{user}:{pw}".encode()).decode()
        req.add_header("Authorization", "Basic " + tok)
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.load(r)


def check(d):
    errs = []
    for k in ("enabled", "hit", "packs", "cloudData", "merged"):
        if k not in d:
            errs.append(f"Schlüssel {k} fehlt")
    h = d.get("hit", {})
    if d.get("enabled") and d.get("hitEnabled") and not (h.get("valid") or h.get("missing")):
        errs.append("HIT: weder Werte noch 'missing' (noch keine Abfrage?)")
    if h.get("valid") and "cellTempMax" not in h:
        errs.append("HIT gültig, aber cellTempMax fehlt")
    m = d.get("merged", {})
    for k, v in m.items():
        if k.startswith("src") and v not in ("hit", "cloud"):
            errs.append(f"merged.{k} = {v!r}")
    return errs


ROWS = [
    # Anzeige, merged, hit, cloudData, Einheit
    ("Zelltemp. max", "cellTempMax", "cellTempMax", "tempMax", "°C"),
    ("Zelltemp. min", "cellTempMin", "cellTempMin", "tempMin", "°C"),
    ("Zellspannung max", "cellVMax", "cellVMax", "cellVMax", "V"),
    ("Zellspannung min", "cellVMin", "cellVMin", "cellVMin", "V"),
    ("Zelldrift", "cellDiffMv", None, "cellDiffMv", "mV"),
    ("Ladegrenze", "chargeLimA", "maxChargeA", "chargeLimA", "A"),
    ("Entladegrenze", "dischargeLimA", "maxDischargeA", "dischargeLimA", "A"),
    ("Zyklen", "cycles", None, "cycles", ""),
    ("Restenergie", "remainKwh", None, "remainKwh", "kWh"),
    ("SoC", None, None, "soc", "%"),
    ("SoH", None, None, "soh", "%"),
    ("Leistung", None, None, "powerW", "W"),
]


def fmt(v):
    return "-" if v is None else str(v)


def table(d):
    m, h, c = d.get("merged", {}), d.get("hit", {}), d.get("cloudData", {})
    print(f"{'Wert':<18}{'Ergebnis':>10}{'HIT':>10}{'Cloud':>10}  Einheit")
    for name, mk, hk, ck, unit in ROWS:
        vals = [m.get(mk) if mk else None, h.get(hk) if hk else None, c.get(ck) if ck else None]
        if all(v is None for v in vals):
            continue
        print(f"{name:<18}" + "".join(f"{fmt(v):>10}" for v in vals) + f"  {unit}")
    print(f"Quelle Zellspannung {m.get('srcCellV', '-')}, Temperatur {m.get('srcTemp', '-')}, "
          f"Grenzen {m.get('srcLim', '-')}")
    print(f"HIT {'gültig' if h.get('valid') else 'keine Werte'}"
          + (f", Alter {h['ageS']} s" if 'ageS' in h else "")
          + (" (Block leer)" if h.get("missing") else ""))
    print(f"Cloud {'gültig' if c.get('valid') else 'keine Werte'}"
          + (f", Alter {c['ageS']} s" if 'ageS' in c else "")
          + (f", Fehler: {c['error']}" if c.get('error') else ""))
    for p in d.get("packs", []):
        print(f"Pack {p.get('sn', '?')}: BMS {p.get('bmsFw', '?')}, Kapazität {p.get('ratedCap', '?')}, "
              f"DCDC {p.get('dcdcSw', '?')}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host", nargs="?")
    ap.add_argument("--user", default="admin")
    ap.add_argument("--pass", dest="pw", default="")
    ap.add_argument("--sign-selftest", action="store_true")
    ap.add_argument("--json", action="store_true", help="Antwort zusätzlich als JSON ausgeben")
    a = ap.parse_args()
    if a.sign_selftest:
        sys.exit(0 if selftest() else 1)
    if not a.host:
        ap.error("host fehlt")
    d = fetch(a.host, a.user, a.pw)
    if a.json:
        print(json.dumps(d, indent=2, ensure_ascii=False))
    table(d)
    errs = check(d)
    for e in errs:
        print("FEHLER:", e)
    print("OK" if not errs else f"{len(errs)} Fehler")
    sys.exit(1 if errs else 0)


if __name__ == "__main__":
    main()
