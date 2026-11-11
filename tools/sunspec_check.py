#!/usr/bin/env python3
"""SunSpec-Kette lesen und optional zwei Quellen vergleichen (nur Standardbibliothek).

  python tools/sunspec_check.py 192.168.4.1
  python tools/sunspec_check.py <nodered-host> --compare <esp-ip>
"""
import argparse
import socket
import struct

BASES = (0, 40000, 50000)
NAMES = {1: "Common", 103: "Inverter 3ph", 120: "Nameplate", 121: "Settings", 122: "Status",
         123: "Controls", 124: "Storage", 160: "MPPT", 201: "Meter 1ph"}
# Felder, die je nach Live-Wert schwanken, beim Vergleich nur informativ
LIVE_OFFSETS = {103: range(0, 50), 124: range(6, 10), 160: range(10, 88), 201: range(0, 50)}


class Client:
    def __init__(self, host, port, unit):
        self.sock = socket.create_connection((host, port), timeout=3)
        self.unit = unit
        self.tid = 0

    def read(self, addr, qty):
        self.tid = (self.tid + 1) & 0xFFFF
        self.sock.sendall(struct.pack(">HHHBBHH", self.tid, 0, 6, self.unit, 3, addr, qty))
        hdr = self._recv(9)
        fc = hdr[7]
        if fc & 0x80:
            raise IOError(f"Exception {hdr[8]} @ {addr}")
        data = self._recv(hdr[8])
        return list(struct.unpack(f">{qty}H", data))

    def _recv(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise IOError("Verbindung geschlossen")
            buf += chunk
        return buf


def find_base(c):
    for base in BASES:
        try:
            if c.read(base, 2) == [0x5375, 0x6E53]:
                return base
        except IOError:
            pass
    return None


def read_chain(c, base):
    models = []
    addr = base + 2
    for _ in range(20):
        mid, length = c.read(addr, 2)
        if mid == 0xFFFF:
            break
        body = []
        off = 0
        while off < length:
            n = min(120, length - off)
            body += c.read(addr + 2 + off, n)
            off += n
        models.append((mid, addr - base, body))
        addr += 2 + length
    return models


def s16(v):
    return v - 0x10000 if v & 0x8000 else v


def summary(models):
    for mid, off, body in models:
        line = f"  @{off:>4}  Model {mid:<4} {NAMES.get(mid, ''):<13} L={len(body)}"
        if mid == 103:
            w = s16(body[12]) * 10 ** s16(body[13])
            wh = ((body[22] << 16) | body[23]) * 10 ** s16(body[24])
            line += f"  W={w:.0f}  WH={wh:.0f}  St={body[36]}"
        if mid == 124:
            line += f"  SoC={body[6] * 10 ** s16(body[20]):.1f}%  ChaSt={body[9]}"
        print(line)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host")
    ap.add_argument("--port", type=int, default=1502)
    ap.add_argument("--unit", type=int, default=1)
    ap.add_argument("--compare", metavar="HOST2")
    a = ap.parse_args()

    results = []
    for host in [a.host] + ([a.compare] if a.compare else []):
        c = Client(host, a.port, a.unit)
        base = find_base(c)
        if base is None:
            print(f"{host}: kein 'SunS' auf {BASES}")
            return 1
        models = read_chain(c, base)
        print(f"{host}: SunSpec ab Basis {base}")
        summary(models)
        results.append(models)

    if len(results) == 2:
        left = {m[0]: m for m in results[0]}
        right = {m[0]: m for m in results[1]}
        print("\nVergleich (statische Register):")
        diffs = 0
        for mid in sorted(set(left) | set(right)):
            if mid not in left or mid not in right:
                print(f"  Model {mid}: nur in {'erster' if mid in left else 'zweiter'} Quelle")
                diffs += 1
                continue
            (_, o1, b1), (_, o2, b2) = left[mid], right[mid]
            if o1 != o2:
                print(f"  Model {mid}: Offset {o1} vs {o2}  <-- Registerverschiebung")
                diffs += 1
            live = LIVE_OFFSETS.get(mid, ())
            for i, (x, y) in enumerate(zip(b1, b2)):
                if x != y and i not in live:
                    print(f"  Model {mid} +{i + 2}: {x} vs {y}")
                    diffs += 1
        print("  identisch" if diffs == 0 else f"  {diffs} Abweichung(en)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
