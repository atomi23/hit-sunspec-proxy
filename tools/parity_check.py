#!/usr/bin/env python3
"""Vergleicht die SunSpec-Karte von Node-RED mit dem, was die ESP-Firmware aus denselben HIT-Daten bauen würde.

Liest nur (HIT: FC4, Node-RED: FC3). Die Umrechnung ist eine 1:1-Kopie von HitClient::doPollTcp
und SunSpecMap::build — bei Änderungen an der Firmware hier mitziehen.

  python tools/parity_check.py --hit 192.0.2.34 --nodered 192.0.2.234
"""
import argparse
import math
import socket
import struct
import time

SF = lambda n: (0x10000 + n) & 0xFFFF


class Modbus:
    def __init__(self, host, port, unit):
        self.s = socket.create_connection((host, port), timeout=5)
        self.unit, self.tid = unit, 0

    def read(self, fc, addr, qty):
        out = []
        while qty:
            n = min(qty, 120)
            self.tid = (self.tid + 1) & 0xFFFF
            self.s.sendall(struct.pack(">HHHBBHH", self.tid, 0, 6, self.unit, fc, addr, n))
            hdr = self._recv(9)
            if hdr[7] & 0x80:
                raise IOError(f"Exception {hdr[8]} bei FC{fc} @{addr}")
            out += struct.unpack(f">{n}H", self._recv(hdr[8]))
            addr, qty = addr + n, qty - n
        return out

    def _recv(self, n):
        b = b""
        while len(b) < n:
            c = self.s.recv(n - len(b))
            if not c:
                raise IOError("Verbindung geschlossen")
            b += c
        return b


i16 = lambda u: u - 0x10000 if u >= 0x8000 else u
i32 = lambda hi, lo: ((hi << 16) | lo) - (1 << 32) if hi >= 0x8000 else (hi << 16) | lo
u32 = lambda hi, lo: (hi << 16) | lo


def poll_hit(host, unit):
    # wie ModbusTcpRaw: je Block eigene Verbindung, der HIT trennt sonst nach wenigen Anfragen
    def rd(addr, qty):
        for attempt in range(3):
            try:
                m = Modbus(host, 502, unit)
                try:
                    return m.read(4, addr, qty)
                finally:
                    m.s.close()
                    time.sleep(0.15)
            except (IOError, OSError) as e:
                err = e
                time.sleep(0.5)
        raise IOError(f"HIT @{addr}x{qty}: {err}")

    mppt = rd(27, 12)
    volt = rd(62, 3)
    freq = rd(66, 1)
    ipb = rd(1804, 14)
    epv = rd(2000, 2)
    eimp = rd(2040, 2)
    eexp = rd(2048, 2)
    bat = rd(30009, 2)
    bi = rd(30014, 12)
    L = {}
    L["pvW"] = sum(mppt[i * 3 + 2] for i in range(4))
    L["v"] = [x * 0.1 for x in volt]
    hz = lambda raw: 4500 <= raw <= 6500
    L["freq"] = ipb[3] * 0.01 if hz(ipb[3]) else (freq[0] * 0.01 if hz(freq[0]) else 50.0)
    L["i"] = [i16(x) * 0.01 for x in ipb[:3]]
    L["gridW"] = -i32(ipb[10], ipb[11])
    L["ePv"] = u32(*epv) * 0.1
    L["eImp"] = u32(*eimp) * 0.1
    L["eExp"] = u32(*eexp) * 0.1
    L["batW"] = i32(*bat)
    soc = bi[6]
    if 100 < soc <= 1000:
        soc //= 10
    L["soc"] = min(soc, 100)
    L["batV"] = bi[0] * 0.1
    return L


def sn16(n):
    return (n + 0x10000) & 0xFFFF if n < 0 else n & 0xFFFF


def put_str(r, off, s, words):
    b = s.encode()[: words * 2].ljust(words * 2, b"\0")
    for i in range(words):
        r[off + i] = (b[2 * i] << 8) | b[2 * i + 1]


def build_map(L, invert=False, wmaxlim=10000, wmaxena=False):
    r = [0] * 512
    pv, bat = L["pvW"], L["batW"]
    if invert:
        pv, bat = -pv, -bat
    braw = L["batW"]
    cha = abs(braw) if braw < -10 else 0
    dis = braw if braw > 10 else 0
    pv_mppt = max(0, abs(pv) - cha - dis)
    if pv < 0:
        pv_mppt = -pv_mppt
    ac = pv + bat
    v1, v2, v3 = [x if x >= 50 else 230 for x in L["v"]]
    i1, i2, i3 = L["i"]
    wh = round(L["ePv"] * 1000) if L["ePv"] > 100 else 0
    wreg = sn16(ac)
    st = 4 if (abs(pv) > 10 or abs(braw) > 10) else 1
    soc = L["soc"]
    chast = 5 if soc >= 99 else 2 if soc <= 1 else 3 if braw > 50 else 4 if braw < -50 else 6
    W, BV = 20000, 400
    ba = round(abs(bat) / BV * 10)

    r[0], r[1], r[2], r[3] = 0x5375, 0x6E53, 1, 66
    put_str(r, 4, "Fronius", 16); put_str(r, 20, "Symo GEN24 20.0 Plus", 16)
    put_str(r, 36, "Hybrid", 8); put_str(r, 44, "0.3.12", 8); put_str(r, 52, "HIT-Proxy", 8)
    r[68] = 1
    r[70], r[71] = 103, 50
    r[72] = round((abs(i1) + abs(i2) + abs(i3)) * 100)
    r[73], r[74], r[75], r[76] = round(abs(i1) * 100), round(abs(i2) * 100), round(abs(i3) * 100), SF(-2)
    r[77], r[78], r[79] = (round(v * 10 * 1.7320508) for v in (v1, v2, v3))
    r[80], r[81], r[82], r[83] = round(v1 * 10), round(v2 * 10), round(v3 * 10), SF(-1)
    r[84] = wreg; r[86], r[87] = 500, SF(-1); r[88] = wreg
    r[92], r[93] = 1000, SF(-1)
    r[94], r[95] = (wh >> 16) & 0xFFFF, wh & 0xFFFF
    r[97], r[98] = round(abs(ac) / max(v1, 1) * 10), SF(-1)
    r[99], r[100], r[101], r[103] = 4000, SF(-1), wreg, 350
    r[104] = r[105] = r[106] = 0x8000; r[107] = SF(-1)
    r[108] = r[109] = st
    r[122], r[123], r[124] = 120, 26, 82
    r[125] = r[127] = r[129] = r[130] = W
    r[131] = r[132] = sn16(-W)
    r[134], r[135] = 320, SF(-1)
    r[136] = r[137] = r[138] = r[139] = 800; r[140] = SF(-3)
    r[141], r[142], r[145], r[147] = 4300, 1, W, W
    r[150], r[151], r[152], r[153] = 121, 30, W, 2300
    r[157] = r[158] = r[159] = W
    r[160] = r[161] = sn16(-W)
    r[163] = r[164] = r[165] = r[166] = 800
    r[173], r[179] = SF(-1), SF(-3)
    r[182], r[183], r[184], r[185], r[186] = 122, 44, 7, 7, 1
    r[228], r[229], r[232] = 123, 24, 1
    r[233], r[237] = wmaxlim or 10000, 1 if wmaxena else 0
    r[251], r[252], r[253] = SF(-2), SF(-3), SF(-2)
    r[254], r[255], r[256], r[257], r[262] = 160, 88, SF(-1), SF(-1), 4

    def mppt(base, mid, dca, dcv, dcw, label):
        r[base] = mid; put_str(r, base + 1, label, 8)
        r[base + 9], r[base + 10], r[base + 11] = sn16(dca), sn16(dcv), sn16(dcw)
        r[base + 16] = 4 if (abs(dcw) > 0 or mid <= 2) else 1

    half = int(pv_mppt / 2)
    rem = pv_mppt - half
    dca = lambda w: round(abs(w) / 40 * 10)
    mppt(264, 1, dca(half), 4000, half, "PV1")
    mppt(284, 2, dca(rem), 4000, rem, "PV2")
    mppt(304, 3, ba if cha else 0, BV * 10, cha, "StCha 3")
    mppt(324, 4, ba if dis else 0, BV * 10, dis, "StDisCha 4")
    r[344], r[345], r[346], r[347], r[348], r[349], r[351] = 124, 24, W, 100, 100, 3, 500
    r[352] = r[353] = soc * 100
    r[354], r[355], r[356], r[357] = BV * 10, chast, 10000, 10000
    r[365] = r[366] = r[369] = SF(-2)
    r[370], r[371] = 0xFFFF, 0
    return r


# Register, die sich mit den Messwerten ändern (Rest muss exakt passen)
LIVE = set(range(72, 76)) | set(range(77, 83)) | {84, 88, 94, 95, 97, 101, 108, 109} | \
    {273, 274, 275, 280, 293, 294, 295, 300, 313, 315, 320, 333, 335, 340, 352, 353, 355} | set(range(44, 60))
NAMES = {72: "A", 73: "AphA", 80: "PhVphA", 84: "W", 88: "VA", 94: "WH hi", 95: "WH lo", 97: "DCA",
         108: "St", 233: "WMaxLimPct", 237: "WMaxLim_Ena", 275: "PV1 DCW", 295: "PV2 DCW",
         315: "StCha DCW", 335: "StDisCha DCW", 352: "SoC", 355: "ChaSt"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hit", required=True)
    ap.add_argument("--hit-unit", type=int, default=1)
    ap.add_argument("--nodered", required=True)
    ap.add_argument("--port", type=int, default=1502)
    a = ap.parse_args()

    L = poll_hit(a.hit, a.hit_unit)
    print("HIT:", {k: (round(v, 2) if isinstance(v, float) else v) for k, v in L.items()})
    nr = Modbus(a.nodered, a.port, 1).read(3, 0, 372)
    esp = build_map(L, wmaxlim=nr[233], wmaxena=bool(nr[237]))

    static_bad, live = [], []
    for i in range(372):
        if nr[i] == esp[i]:
            continue
        (live if i in LIVE else static_bad).append(i)
    print(f"\nStatische Register mit Abweichung: {len(static_bad)}")
    for i in static_bad:
        print(f"  @{i:3d}  Node-RED {nr[i]:5d} (0x{nr[i]:04x})   ESP {esp[i]:5d} (0x{esp[i]:04x})")
    print(f"\nMesswert-Register (Zeitversatz möglich): {len(live)}")
    for i in live:
        if 44 <= i < 60:
            continue
        print(f"  @{i:3d} {NAMES.get(i, ''):13s} Node-RED {i16(nr[i]):7d}   ESP {i16(esp[i]):7d}")
    wh_nr, wh_esp = u32(nr[94], nr[95]), u32(esp[94], esp[95])
    print(f"\nWH: Node-RED {wh_nr / 1000:.1f} kWh, ESP {wh_esp / 1000:.1f} kWh")


if __name__ == "__main__":
    main()
