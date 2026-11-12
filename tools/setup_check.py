#!/usr/bin/env python3
"""Einrichtung eines frisch gelöschten Proxys per Kabel durchspielen (Demo-Modus, keine echten Daten).

  python tools/setup_check.py 192.0.2.80 boot.log

Den Zugangscode liest das Skript aus dem seriellen Startlog (boot.log), es gibt ihn nicht aus.
"""
import base64
import http.client
import json
import re
import ssl
import sys
import time

host, log = sys.argv[1], sys.argv[2]
m = re.search(r'Display-Code:?\s+"?([A-Za-z0-9]+)', open(log, encoding="utf-8", errors="replace").read())
if not m:
    print("Zugangscode nicht im Log gefunden")
    sys.exit(2)
auth = {"Authorization": "Basic " + base64.b64encode(f"admin:{m.group(1)}".encode()).decode(),
        "Content-Type": "application/json"}

c = http.client.HTTPConnection(host, 80, timeout=10)
c.request("GET", "/setup", headers=auth)
r = c.getresponse()
r.read()
print("Einrichtung per Kabel erreichbar:", r.status)

body = {"hitHost": "192.0.2.50", "hitPort": 502, "hitUnitId": 1, "hitMode": 0, "demoMode": True,
        "sunspecPort": 1502, "sunspecUnitId": 1, "setupComplete": True, "wifiApMode": False}
t = time.time()
c = http.client.HTTPConnection(host, 80, timeout=20)
c.request("POST", "/api/setup/finish", body=json.dumps(body), headers=auth)
r = c.getresponse()
ans = r.read().decode()
print(f"Speichern: HTTP {r.status} nach {(time.time() - t) * 1000:.0f} ms: {ans}")

time.sleep(3)
c = http.client.HTTPConnection(host, 80, timeout=10)
c.request("GET", "/")
r = c.getresponse()
r.read()
print("danach HTTP /:", r.status, r.getheader("Location"))
c = http.client.HTTPSConnection(host, 443, context=ssl._create_unverified_context(), timeout=15)
c.request("GET", "/api/status", headers=auth)
r = c.getresponse()
s = json.loads(r.read())
print("HTTPS /api/status:", r.status, "Einrichtung abgeschlossen:", s.get("setupComplete"), "Quelle:", s.get("dataOrigin"))
