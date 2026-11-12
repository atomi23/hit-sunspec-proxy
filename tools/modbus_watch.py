#!/usr/bin/env python3
"""Liest regelmäßig die SunSpec-Kennung (Register 40000/40001 = "SunS") und Model 103 vom Proxy.

  python tools/modbus_watch.py 192.0.2.80 --port 1502 --secs 7200 --every 10

Prüft, dass Consolinno während anderer Last weiter Antworten bekommt. Eine Zeile pro Minute.
"""
import argparse
import socket
import struct
import sys
import time


def read_holding(host: str, port: int, unit: int, addr: int, qty: int, timeout: float) -> list[int]:
    with socket.create_connection((host, port), timeout=timeout) as s:
        s.settimeout(timeout)
        s.sendall(struct.pack(">HHHBBHH", 1, 0, 6, unit, 3, addr, qty))
        hdr = b""
        while len(hdr) < 9:
            chunk = s.recv(9 - len(hdr))
            if not chunk:
                raise ConnectionError("getrennt")
            hdr += chunk
        if hdr[7] & 0x80:
            raise ValueError(f"Exception {hdr[8]}")
        n = hdr[8]
        data = b""
        while len(data) < n:
            chunk = s.recv(n - len(data))
            if not chunk:
                raise ConnectionError("getrennt")
            data += chunk
        return list(struct.unpack(f">{n // 2}H", data))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=1502)
    ap.add_argument("--unit", type=int, default=1)
    ap.add_argument("--secs", type=float, default=600)
    ap.add_argument("--every", type=float, default=10)
    a = ap.parse_args()

    end = time.time() + a.secs
    ok = fail = 0
    m_ok = m_fail = 0
    last = time.time()
    worst = 0.0
    while time.time() < end:
        t = time.time()
        try:
            regs = read_holding(a.host, a.port, a.unit, 40000, 2, 5)
            if regs != [0x5375, 0x6E53]:
                raise ValueError(f"keine SunSpec-Kennung: {regs}")
            ok += 1
            m_ok += 1
            worst = max(worst, (time.time() - t) * 1000)
        except Exception as e:  # noqa: BLE001
            fail += 1
            m_fail += 1
            print(time.strftime("%H:%M:%S"), "FEHLER", type(e).__name__, e, flush=True)
        if time.time() - last >= 60:
            print(f"{time.strftime('%H:%M:%S')} Modbus ok {m_ok} Fehler {m_fail} langsamste {worst:.0f} ms", flush=True)
            last, m_ok, m_fail, worst = time.time(), 0, 0, 0.0
        time.sleep(max(0.0, a.every - (time.time() - t)))
    print(f"--- Modbus {a.host}:{a.port}: {ok} ok, {fail} Fehler")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
