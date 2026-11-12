#!/usr/bin/env python3
"""Funktionsprüfung der HTTPS-Seite des Proxys.

  python tools/https_check.py 192.0.2.80 [--ota .pio/build/t-connect-pro/firmware.bin]

Prüft Umleitung, Einstellungen speichern, Zertifikat hochladen (falscher Schlüssel muss abgelehnt werden),
Rückkehr zum selbst signierten Zertifikat und optional ein OTA-Update über HTTPS.
Zugangsdaten aus ESP_USER / ESP_PASS. Am Ende ist wieder das selbst signierte Zertifikat aktiv.
"""
import argparse
import base64
import datetime
import hashlib
import http.client
import ipaddress
import json
import os
import socket
import ssl
import sys
import time
import uuid

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.x509.oid import NameOID

CTX = ssl._create_unverified_context()
results = []


def check(name: str, ok: bool, detail: str = "") -> None:
    results.append(ok)
    print(f"{'OK    ' if ok else 'FEHLER'} {name}{(' — ' + detail) if detail else ''}", flush=True)


def served_cert(host: str) -> x509.Certificate:
    with socket.create_connection((host, 443), timeout=15) as s, CTX.wrap_socket(s) as t:
        return x509.load_der_x509_certificate(t.getpeercert(True))


def req(host, auth, method, path, body=None, ctype="application/json", timeout=30):
    c = http.client.HTTPSConnection(host, 443, context=CTX, timeout=timeout)
    h = dict(auth)
    if body is not None:
        h["Content-Type"] = ctype
    c.request(method, path, body=body, headers=h)
    r = c.getresponse()
    data = r.read()
    c.close()
    return r.status, data


def make_cert(host: str, key):
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "test.hit-proxy.example")])
    now = datetime.datetime.now(datetime.timezone.utc)
    return (x509.CertificateBuilder().subject_name(name).issuer_name(name).public_key(key.public_key())
            .serial_number(x509.random_serial_number())
            .not_valid_before(now - datetime.timedelta(days=1)).not_valid_after(now + datetime.timedelta(days=90))
            .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address(host))]), False)
            .sign(key, hashes.SHA256()))


def pem_key(key) -> str:
    return key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                             serialization.NoEncryption()).decode()


def wait_https(host: str, secs: float) -> bool:
    end = time.time() + secs
    while time.time() < end:
        try:
            served_cert(host)
            return True
        except Exception:  # noqa: BLE001
            time.sleep(1)
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--ota", help="firmware.bin per HTTPS hochladen (Gerät startet neu)")
    a = ap.parse_args()
    user, pw = os.environ.get("ESP_USER", "admin"), os.environ.get("ESP_PASS", "")
    if not pw:
        print("ESP_PASS fehlt")
        return 2
    auth = {"Authorization": "Basic " + base64.b64encode(f"{user}:{pw}".encode()).decode()}
    host = a.host

    # Umleitung
    c = http.client.HTTPConnection(host, 80, timeout=10)
    c.request("GET", "/a.css")
    r = c.getresponse()
    r.read()
    check("HTTP leitet auf HTTPS um", r.status in (301, 302) and (r.getheader("Location") or "").startswith("https://"),
          f"{r.status} {r.getheader('Location')}")

    st, body = req(host, {}, "GET", "/api/status")
    check("ohne Anmeldung gesperrt", st == 401, str(st))

    st, body = req(host, auth, "GET", "/api/tls")
    info = json.loads(body)
    cert = served_cert(host)
    fp = ":".join(f"{b:02X}" for b in hashlib.sha256(cert.public_bytes(serialization.Encoding.DER)).digest())
    check("Fingerabdruck in der Oberfläche = ausgeliefertes Zertifikat", info.get("sha256") == fp, info.get("source", ""))

    # Einstellungen speichern über HTTPS (gleicher Wert, keine Änderung)
    st, body = req(host, auth, "GET", "/api/config")
    cfg = json.loads(body)
    st, body = req(host, auth, "POST", "/api/config", json.dumps({"displayTimeoutS": cfg["displayTimeoutS"]}))
    check("Einstellungen speichern", st == 200 and json.loads(body).get("ok"), body[:60].decode(errors="replace"))

    # eigenes Zertifikat: erst mit falschem Schlüssel, dann richtig
    key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    other = ec.generate_private_key(ec.SECP256R1())
    crt = make_cert(host, key).public_bytes(serialization.Encoding.PEM).decode()
    st, body = req(host, auth, "POST", "/api/tls/cert", json.dumps({"cert": crt, "key": pem_key(other)}))
    check("falscher Schlüssel wird abgelehnt", st == 400, json.loads(body).get("error", ""))
    st, body = req(host, auth, "POST", "/api/tls/cert", json.dumps({"cert": "kein pem", "key": "x"}))
    check("Unsinn wird abgelehnt", st == 400, json.loads(body).get("error", ""))
    st, body = req(host, auth, "POST", "/api/tls/cert", json.dumps({"cert": crt, "key": pem_key(key)}))
    check("eigenes Zertifikat angenommen", st == 200, body[:80].decode(errors="replace"))
    time.sleep(4)
    ok = wait_https(host, 20)
    cn = served_cert(host).subject.rfc4514_string() if ok else "-"
    check("HTTPS liefert das eigene Zertifikat", "test.hit-proxy.example" in cn, cn)
    st, body = req(host, auth, "GET", "/api/tls")
    check("Oberfläche zeigt eigenes Zertifikat", json.loads(body).get("source") == "own")

    # zurück zum selbst signierten
    st, body = req(host, auth, "POST", "/api/tls/self")
    time.sleep(6)
    ok = wait_https(host, 30)
    issuer = served_cert(host).issuer.rfc4514_string() if ok else "-"
    check("wieder selbst signiert", ok and "HIT-SunSpec-Proxy" in issuer, issuer)

    if a.ota:
        data = open(a.ota, "rb").read()
        boundary = uuid.uuid4().hex
        body = (f"--{boundary}\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"firmware.bin\"\r\n"
                f"Content-Type: application/octet-stream\r\n\r\n").encode() + data + f"\r\n--{boundary}--\r\n".encode()
        t = time.time()
        try:
            st, resp = req(host, auth, "POST", "/api/ota", body, f"multipart/form-data; boundary={boundary}", timeout=180)
        except Exception as e:  # noqa: BLE001
            st, resp = 0, str(e).encode()
        check("OTA über HTTPS", st == 200, f"{len(data)} Bytes in {time.time() - t:.0f} s, {resp[:60].decode(errors='replace')}")
        time.sleep(20)
        ok = wait_https(host, 90)
        st, body = req(host, auth, "GET", "/api/status") if ok else (0, b"{}")
        s = json.loads(body)
        check("nach OTA wieder erreichbar", ok and s.get("uptimeS", 9999) < 200, f"Laufzeit {s.get('uptimeS')} s")

    print(f"--- {sum(results)}/{len(results)} bestanden")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
