#!/usr/bin/env python3
"""Displaybilder (Live, Strings und Netz) vom Proxy als PNG holen, für die Doku.

  python tools/capture_display.py 192.0.2.80 boot.log docs/screenshots

Den Zugangscode liest das Skript aus dem seriellen Startlog. Ist die Einrichtung offen, schließt es sie im
Demo-Modus ab (Fake-Daten), damit keine echten Anlagenwerte auf den Bildern stehen. Die Systemseite wird
nicht gespeichert, weil sie die Adresse im Heimnetz zeigt.
"""
import base64
import http.client
import json
import re
import ssl
import sys
import time

from PIL import Image

host, log, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
m = re.search(r'Display-Code:?\s+"?([A-Za-z0-9]+)', open(log, encoding="utf-8", errors="replace").read())
if not m:
    sys.exit("Zugangscode nicht im Log gefunden")
auth = {"Authorization": "Basic " + base64.b64encode(f"admin:{m.group(1)}".encode()).decode()}
ctx = ssl._create_unverified_context()


def req(method, path, body=None, https=True):
    c = (http.client.HTTPSConnection(host, 443, context=ctx, timeout=20) if https
         else http.client.HTTPConnection(host, 80, timeout=20))
    h = dict(auth)
    if body is not None:
        h["Content-Type"] = "application/json"
    c.request(method, path, body=body, headers=h)
    r = c.getresponse()
    return r, r.read()


r, b = req("GET", "/api/status", https=False)
if r.status == 200 and not json.loads(b).get("setupComplete", True):
    body = {"hitHost": "192.0.2.50", "hitPort": 502, "hitUnitId": 1, "hitMode": 0, "demoMode": True,
            "sunspecPort": 1502, "sunspecUnitId": 1, "setupComplete": True, "wifiApMode": False}
    r, b = req("POST", "/api/setup/finish", json.dumps(body), https=False)
    print("Einrichtung im Demo-Modus:", r.status)
    time.sleep(15)

for name in ("display-live", "display-strings"):
    time.sleep(3)
    r, raw = req("GET", "/api/screen")
    w, h = int(r.getheader("X-Width")), int(r.getheader("X-Height"))
    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        v = (raw[2 * i] << 8) | raw[2 * i + 1]
        px[i % w, i // w] = (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
    img.save(f"{outdir}/{name}.png")
    print("gespeichert", name, w, "x", h)
    req("POST", "/api/display/tap")
