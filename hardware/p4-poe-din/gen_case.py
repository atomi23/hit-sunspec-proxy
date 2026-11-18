#!/usr/bin/env python3
"""Hutschienengehäuse (3,5 TE) für Waveshare ESP32-P4-WIFI6-POE-ETH, ohne Schrauben.

Die Platine (55 x 68 mm) steht parallel zur Hutschiene, Bauteile nach vorn, LAN und USB oben.
- Hutschienen-Clip am Unterteil angedruckt: Haken oben fest, unten federnder Riegel. Die Feder ist
  eine stehende Platte unten im Gehäuse, ein Steg hinter der Platine verbindet sie mit dem Riegel.
- Die Platine wird von oben (Deckelseite) in zwei Nuten gesteckt: eine an der Seitenwand des
  Unterteils, eine im Deckel. Unten liegt sie auf zwei Auflagen, oben stehen LAN und USB im Ausschnitt.
- Der Deckel rastet mit sechs Federzungen ein.

    python gen_case.py
    solidcraft-cli run case.json --save p4-poe-din.sc --script-relative

Koordinaten (mm): X = Tiefe (0 = Anlagefläche an der Schiene), Y = Höhe (0 = unten),
Z = Breite (0 = Außenseite der Seitenwand des Unterteils, liegt beim Druck auf dem Bett).
Lage der Buchsen aus der Maßzeichnung von Waveshare/Amazon abgelesen: vor dem Druck nachmessen.
"""
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent

# --- Gehäuse ---------------------------------------------------------------
W = 2.0
D = 47.0           # Tiefe Grundkörper ab Schiene
H = 90.0
ZB = 59.5          # Unterteil
LID = 2.5          # -> 62 mm Breite = 3,5 TE
STEP_D = 14.0
STEP_Y0, STEP_Y1 = 22.5, 67.5

# --- ESP32-P4-WIFI6-POE-ETH ----------------------------------------------------
BW, BH, BT = 55.0, 68.0, 1.6   # Breite (Z), Höhe (Y), Dicke
BX = 10.0                      # Rückseite der Platine
BZ0 = W + 0.3                  # Kante an der Seitenwand des Unterteils
PORT_OVH = 3.0                 # LAN/USB stehen über die obere Platinenkante
BY1 = H - W - 0.3 - PORT_OVH   # obere Platinenkante (Buchsen enden 0,3 vor der Wand)
BY0 = BY1 - BH
# Ausschnitte oben, Z ab der Platinenkante (Zeichnung: USB 4,6..18,2, LAN-Mitte 29,9), X-Höhe über der Platine
USB_Z, USB_HT = (4.5, 20.3), 16.5
RJ_Z, RJ_HT = (21.5, 38.1), 13.5
GR = 1.2                       # Nuttiefe: FPC-Buchsen stehen ab 2 mm von der Kante
PM = 0.7                       # Luft um die Buchsen

# --- Hutschiene ----------------------------------------------------------------
RAIL_YC = 45.0
RY0, RY1 = RAIL_YC - 17.5, RAIL_YC + 17.5
LIP_F = -1.3
LIP_B = LIP_F - 1.7

cmds = []


def c(command, **params):
    cmds.append({"command": command, "params": params})


def box(x0, x1, y0, y1, z0, z1, body=None, op=None, target=None):
    p = {"length": round(x1 - x0, 3), "width": round(y1 - y0, 3), "height": round(z1 - z0, 3),
         "corner": [x0, y0, z0]}
    if body:
        p["body_name"] = body
    if op:
        p["operation"] = op
        p["targets"] = [target]
    c("solid.box", **p)


def cyl(d, h, base, axis, op, target):
    c("solid.cylinder", diameter=d, height=h, base=base, axis=axis, operation=op, targets=[target])


def prism(plane, points, a0, a1, op, target):
    c("sketch.create", plane=plane, offset=a0)
    c("sketch.line", points=points, closed=True)
    c("sketch.finish")
    c("solid.extrude", distance=round(a1 - a0, 3), operation=op, targets=[target])


B = "Unterteil"
L = "Deckel"

# ===== Unterteil ===========================================================
box(0, D, 0, H, 0, ZB, body=B)
box(D, D + STEP_D, STEP_Y0, STEP_Y1, 0, ZB, op="join", target=B)
box(W, D - W, W, H - W, W, ZB + 1, op="cut", target=B)
box(D - W - 1, D + STEP_D - W, STEP_Y0 + W, STEP_Y1 - W, W, ZB + 1, op="cut", target=B)
for z in (8.0, 18.0, 28.0, 38.0, 48.0):
    box(D + STEP_D - W - 1, D + STEP_D + 1, 32, 58, z, z + 1.6, op="cut", target=B)

# LAN und USB oben
front = BX + BT
for (z0, z1), ht in ((USB_Z, USB_HT), (RJ_Z, RJ_HT)):
    box(front - PM, front + ht + PM, H - W - 1, H + 1, BZ0 + z0 - PM, BZ0 + z1 + PM, op="cut", target=B)

# Nut an der Seitenwand (Platinenkante steht 0,3 über der Wand) und Auflagen unter der Unterkante
for y0 in (BY0 + 12, BY0 + 40):
    box(BX - 1.5, BX - 0.15, y0, y0 + 14, W - 0.01, BZ0 + GR, op="join", target=B)
    box(front + 0.15, front + 1.5, y0, y0 + 14, W - 0.01, BZ0 + GR, op="join", target=B)
box(BX, front, BY0 - 1.5, BY0 - 0.2, W - 0.01, BZ0 + 6, op="join", target=B)
# Anschlag über der oberen Kante neben den Buchsen, damit Ziehen am Kabel die Platine nicht hebt
box(BX, front, BY1 + 0.2, H - W + 0.01, W - 0.01, BZ0 + 3.5, op="join", target=B)

# ---- fester Haken oben -------------------------------------------------------
box(LIP_F, 0.01, RY1 + 0.1, RY1 + 3.5, 1.0, ZB - 1.0, op="join", target=B)
prism("XY", [[LIP_F, RY1 + 3.5], [LIP_B, RY1 + 3.5], [LIP_B, RY1 - 2.0], [LIP_F, RY1 - 3.0]],
      1.0, ZB - 1.0, "join", B)

# ---- federnder Riegel unten ----------------------------------------------------
# Freiräume durch Seitenwand und Innenraum: Rückwand am Riegel, Steg hinter der Platine, Federplatte
SPR_X1 = 32.0
box(-1, W, 19.4, RY0 - 0.1, -1, ZB + 1, op="cut", target=B)
box(W, BX - 3.0, 7.0, RY0 - 0.1, -1, ZB + 1, op="cut", target=B)
box(W, SPR_X1, 6.0, 11.6, -1, ZB + 1, op="cut", target=B)
box(SPR_X1, SPR_X1 + 4, 5.0, 15.0, W - 0.01, ZB - 0.6, op="join", target=B)   # Anker

mz0, mz1 = 0.0, ZB - 0.6
box(LIP_B, 6.0, 22.8, RY0 - 0.5, mz0, mz1, op="join", target=B)          # Riegelkörper
box(3.0, 6.0, 9.4, 22.81, mz0, mz1, op="join", target=B)                 # Steg nach unten
box(3.0, SPR_X1 + 0.5, 9.4, 11.0, mz0, mz1, op="join", target=B)         # Federplatte 1,6 mm
prism("XY", [[LIP_F, RY0 - 0.51], [LIP_F, RY0 + 3.0], [LIP_B, RY0 - 0.5], [LIP_B, RY0 - 0.51]],
      mz0, mz1, "join", B)                                                # Nase mit Anlaufschräge
box(LIP_B, -0.6, -1.0, 22.81, mz0, mz1, op="join", target=B)             # Zugsteg hinter der Rückwand
box(LIP_B, 8.0, -7.0, -1.0, mz0, mz1, op="join", target=B)               # Lasche
cyl(3.2, 13, [LIP_B - 1, -4.0, (mz0 + mz1) / 2], "X", "cut", B)

# ---- Rastfenster -------------------------------------------------------------
FL, FT, FCL, BD = 10.0, 1.5, 0.2, 0.9
zt = ZB - FL
TOP = [(33.0, 41.0)]                     # oben ist links der Platz der Buchsen
BOT = [(12.0, 20.0), (35.0, 43.0)]
FRONT = [(6.0, 14.0), (76.0, 84.0)]      # Vorderwand neben der Stufe (Y-Bereiche)
for x0, x1 in TOP + [(3.0, 9.0)]:
    box(x0 - 0.3, x1 + 0.3, H - W - 1, H + 1, zt - 0.3, zt + 2.5, op="cut", target=B)
for x0, x1 in BOT:
    box(x0 - 0.3, x1 + 0.3, -1, W + 1, zt - 0.3, zt + 2.5, op="cut", target=B)
for y0, y1 in FRONT:
    box(D - W - 1, D + 1, y0 - 0.3, y1 + 0.3, zt - 0.3, zt + 2.5, op="cut", target=B)

# ===== Deckel ==============================================================
box(0, D, 0, H, ZB, ZB + LID, body=L)
box(D, D + STEP_D, STEP_Y0, STEP_Y1, ZB, ZB + LID, op="join", target=L)


def tab_y(x0, x1, y_face, sign):
    """Zunge an einer Wand quer zu Y; sign +1 = Rastnase nach +Y."""
    yi = y_face - sign * FT
    box(x0, x1, min(y_face, yi), max(y_face, yi), zt, ZB + 0.01, op="join", target=L)
    prism("YZ", [[y_face - sign * 0.01, zt], [y_face + sign * BD, zt + 1.4], [y_face + sign * BD, zt + 2.4],
                 [y_face - sign * 0.01, zt + 2.4]], x0, x1, "join", L)


for x0, x1 in TOP + [(3.0, 9.0)]:
    tab_y(x0, x1, H - W - FCL, +1)
for x0, x1 in BOT:
    tab_y(x0, x1, W + FCL, -1)
for y0, y1 in FRONT:
    xf = D - W - FCL
    box(xf - FT, xf, y0, y1, zt, ZB + 0.01, op="join", target=L)
    prism("XZ", [[xf - 0.01, -zt], [xf + BD, -(zt + 1.4)], [xf + BD, -(zt + 2.4)], [xf - 0.01, -(zt + 2.4)]],
          y0, y1, "join", L)

# Nut für die zweite Platinenkante: Rippen vorn/hinten, Anschlag als Grund
edge = BZ0 + BW
for y0 in (BY0 + 12, BY0 + 40):
    box(BX - 1.5, BX - 0.15, y0, y0 + 14, edge - GR, ZB + 0.01, op="join", target=L)
    box(front + 0.15, front + 1.5, y0, y0 + 14, edge - GR, ZB + 0.01, op="join", target=L)
    box(BX, front, y0, y0 + 14, edge + 0.2, ZB + 0.01, op="join", target=L)
box(BX, front, BY0 - 1.5, BY0 - 0.2, edge - 6, ZB + 0.01, op="join", target=L)   # Auflage unten
box(BX, front, BY1 + 0.2, H - W - 0.2, edge - 3.0, ZB + 0.01, op="join", target=L)  # Anschlag oben

# ===== Export ===============================================================
(HERE / "stl").mkdir(exist_ok=True)
c("file.save_mesh", path="stl/unterteil.stl", bodies=[B], quality="fine")
c("solid.move", bodies=[L], axis=[1, 0, 0], angle="180 deg")
c("file.save_mesh", path="stl/deckel.stl", bodies=[L], quality="fine")
c("solid.move", bodies=[L], axis=[1, 0, 0], angle="-180 deg")

(HERE / "case.json").write_text(json.dumps({"commands": cmds}, indent=1), encoding="utf-8")
print(f"{len(cmds)} Befehle -> case.json")
