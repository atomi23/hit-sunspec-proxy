#!/usr/bin/env python3
"""Lasttest für die Weboberfläche des Proxys (HTTP oder HTTPS).

Beispiele:
  python tools/esp_soak.py 192.0.2.80 --secs 300 --parallel 4
  python tools/esp_soak.py 192.0.2.80 --https --secs 7200 --parallel 2 --pause 2

Zugangsdaten für die geschützten Seiten aus ESP_USER / ESP_PASS (sonst nur öffentliche Pfade).
Jede Minute eine Zeile mit Erfolgen, Fehlern und Antwortzeiten; am Ende eine Zusammenfassung.
"""
import argparse
import base64
import http.client
import json
import os
import ssl
import statistics
import sys
import threading
import time
from collections import Counter

PUBLIC = ["/a.css", "/f/barlow-400.woff", "/f/barlow-600.woff"]
AUTHED = ["/", "/api/status", "/api/history/day", "/history", "/api/events"]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--https", action="store_true")
    ap.add_argument("--secs", type=float, default=120)
    ap.add_argument("--parallel", type=int, default=2)
    ap.add_argument("--pause", type=float, default=0.5, help="Pause je Worker zwischen Anfragen (s)")
    ap.add_argument("--timeout", type=float, default=15)
    ap.add_argument("--keepalive", action="store_true", help="Verbindung je Worker offen halten wie ein Browser")
    ap.add_argument("--paths", default="", help="Kommagetrennte Pfade statt der Standardliste")
    a = ap.parse_args()

    user, pw = os.environ.get("ESP_USER", ""), os.environ.get("ESP_PASS", "")
    auth = {"Authorization": "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode()} if pw else {}
    paths = a.paths.split(",") if a.paths else PUBLIC + (AUTHED if auth else [])
    ctx = ssl._create_unverified_context()

    lock = threading.Lock()
    lat, errs, codes = [], Counter(), Counter()
    minute = {"ok": 0, "fail": 0, "lat": []}
    end = time.time() + a.secs
    local = threading.local()

    def conn():
        return (http.client.HTTPSConnection(a.host, 443, context=ctx, timeout=a.timeout) if a.https
                else http.client.HTTPConnection(a.host, 80, timeout=a.timeout))

    def fetch(path: str):
        if not a.keepalive:
            c = conn()
            try:
                c.request("GET", path, headers=auth)
                r = c.getresponse()
                r.read()
                return r
            finally:
                c.close()
        # wie ein Browser: bestehende Verbindung nutzen, bei Abbruch einmal neu aufbauen
        for attempt in range(2):
            if getattr(local, "c", None) is None:
                local.c = conn()
            try:
                local.c.request("GET", path, headers=auth)
                r = local.c.getresponse()
                r.read()
                if r.getheader("Connection", "").lower() == "close":
                    local.c.close()
                    local.c = None
                return r
            except (ConnectionError, http.client.RemoteDisconnected, http.client.CannotSendRequest,
                    http.client.BadStatusLine, ssl.SSLEOFError):
                local.c.close()
                local.c = None
                if attempt:
                    raise
        raise RuntimeError("unreachable")

    def one(path: str) -> None:
        t = time.time()
        try:
            r = fetch(path)
            ms = (time.time() - t) * 1000
            with lock:
                codes[r.status] += 1
                if r.status in (200, 302):
                    lat.append(ms)
                    minute["ok"] += 1
                    minute["lat"].append(ms)
                else:
                    errs[f"HTTP {r.status} {path}"] += 1
                    minute["fail"] += 1
        except Exception as e:  # noqa: BLE001 — jede Art Fehlschlag zählt
            with lock:
                errs[type(e).__name__] += 1
                minute["fail"] += 1

    def worker(i: int) -> None:
        n = i
        while time.time() < end:
            one(paths[n % len(paths)])
            n += 1
            time.sleep(a.pause)

    th = [threading.Thread(target=worker, args=(i,), daemon=True) for i in range(a.parallel)]
    for t in th:
        t.start()
    last = time.time()
    while any(t.is_alive() for t in th):
        time.sleep(1)
        if time.time() - last >= 60 or not any(t.is_alive() for t in th):
            last = time.time()
            with lock:
                m = minute.copy()
                minute.update(ok=0, fail=0, lat=[])
            p50 = statistics.median(m["lat"]) if m["lat"] else 0
            print(f"{time.strftime('%H:%M:%S')} ok {m['ok']:4d}  Fehler {m['fail']:3d}  median {p50:6.0f} ms", flush=True)

    total_ok = len(lat)
    total_fail = sum(errs.values())
    print("---")
    print(f"{'HTTPS' if a.https else 'HTTP'} {a.host}: {total_ok} ok, {total_fail} Fehler")
    if lat:
        q = statistics.quantiles(lat, n=20) if len(lat) > 20 else [max(lat)] * 19
        print(f"Antwortzeit median {statistics.median(lat):.0f} ms, p95 {q[18]:.0f} ms, max {max(lat):.0f} ms")
    for k, v in errs.most_common(8):
        print(f"  {v:5d} × {k}")
    if auth:
        try:
            c = (http.client.HTTPSConnection(a.host, 443, context=ctx, timeout=a.timeout) if a.https
                 else http.client.HTTPConnection(a.host, 80, timeout=a.timeout))
            c.request("GET", "/api/status", headers=auth)
            s = json.loads(c.getresponse().read())
            print(f"ESP: Laufzeit {s.get('uptimeS')} s, interner RAM frei {s.get('heapInternal')}, "
                  f"Minimum {s.get('heapInternalMin')}, Neustartgrund {s.get('resetReason')}")
        except Exception as e:  # noqa: BLE001
            print("ESP-Status nicht abrufbar:", type(e).__name__)
    return 1 if total_fail else 0


if __name__ == "__main__":
    sys.exit(main())
