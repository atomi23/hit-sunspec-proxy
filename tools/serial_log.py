#!/usr/bin/env python3
"""Seriellen Log des ESP mitschreiben, ohne ihn beim Öffnen zurückzusetzen.

  python tools/serial_log.py COM3 120 [Datei]

Achtung: Beim Schließen des USB-Ports kann der ESP trotzdem neu starten (USB-CDC).
"""
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM3"
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 60
out = open(sys.argv[3], "w", encoding="utf-8") if len(sys.argv) > 3 else sys.stdout

s = serial.Serial()
s.port, s.baudrate, s.timeout = port, 115200, 0.2
s.dtr = False
s.rts = False
s.open()
end = time.time() + secs
pending = b""
while time.time() < end:
    pending += s.read(4096)
    *lines, pending = pending.split(b"\n")
    for ln in lines:
        out.write(time.strftime("%H:%M:%S ") + ln.decode("utf-8", "replace").rstrip("\r") + "\n")
    out.flush()
s.close()
