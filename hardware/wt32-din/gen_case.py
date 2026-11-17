#!/usr/bin/env python3
"""Hutschienengehäuse (2 TE) für WT32-ETH01 + Step-down 12-24 V -> 5 V, optional RS485-Modul.

Erzeugt ein SolidCraft-Skript (case.json). Bauen und exportieren:

    solidcraft-cli run case.json --save wt32-din.sc

Das Skript schreibt die STL-Dateien selbst (file.save_mesh) nach stl/.

Koordinaten (mm): X = Tiefe (0 = Rückseite zur Hutschiene, + nach vorn), Y = Höhe (0 = unten,
LAN-Buchse zeigt nach unten), Z = Breite über die Schiene (0 = Seitenwand des Unterteils).
Alle Maße der Platine sind Datenblatt-/Fotowerte; vor dem ersten Druck mit der echten Platine prüfen.
"""
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
STL = HERE / "stl"

# --- Gehäuse ---------------------------------------------------------------
W = 2.0            # Wandstärke
D = 44.0           # Tiefe Grundkörper (bis zur Abdeckungskante)
H = 90.0           # Höhe (DIN 43880: 90 mm)
ZB = 33.5          # Höhe Unterteil (Seitenwand + Wände)
LID = 2.5          # Deckelstärke -> Gesamtbreite 36 mm = 2 TE
STEP_D = 14.0      # Frontstufe (ragt durch den 45-mm-Ausschnitt der Verteilerabdeckung)
STEP_Y0, STEP_Y1 = 22.5, 67.5

# --- WT32-ETH01 (60 x 26 mm, LAN-Buchse an der kurzen Seite) ----------------
PCB_X0 = 9.0       # Platinenkante zur Rückwand
PCB_W = 26.0
PCB_L = 60.0
PCB_Z = 15.0       # Unterseite Platine (Platz für Stiftleisten + angelötete Litzen darunter)
PCB_T = 1.6
RJ_W = 18.0        # Ausschnitt LAN-Buchse (Buchse ca. 16 x 13,5), mittig auf der Platine
RJ_H = 14.4

# --- Hutschiene TS35 / Clip --------------------------------------------------
RAIL_YC = 45.0     # Mitte der Schiene
CLIP_T = 3.0       # Clip-Platte
FL_T = 1.0         # Blechstärke Schiene
CLR = 0.3

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


def prism_xy(points, z0, z1, op, target):
    """Polygon in der XY-Ebene, von z0 bis z1 extrudiert."""
    c("sketch.create", plane="XY", offset=z0)
    c("sketch.line", points=points, closed=True)
    c("sketch.finish")
    c("solid.extrude", distance=round(z1 - z0, 3), operation=op, targets=[target])


B = "Unterteil"
L = "Deckel"
K = "Clip"

# ===== Unterteil ===========================================================
box(0, D, 0, H, 0, ZB, body=B)
box(D, D + STEP_D, STEP_Y0, STEP_Y1, 0, ZB, op="join", target=B)
box(W, D - W, W, H - W, W, ZB + 1, op="cut", target=B)
box(D - W - 1, D + STEP_D - W, STEP_Y0 + W, STEP_Y1 - W, W, ZB + 1, op="cut", target=B)

# Schraubdome für den Deckel (Blechschraube 2,9 bzw. M3 selbstschneidend)
POSTS = [(5, 5), (D - 5, 5), (5, H - 5), (D - 5, H - 5)]
for x, y in POSTS:
    cyl(6, ZB - W, [x, y, W], "Z", "join", B)
    cyl(2.5, 12, [x, y, ZB - 12], "Z", "cut", B)

# LAN-Buchse durch den Boden
rj_x0 = PCB_X0 + (PCB_W - RJ_W) / 2
box(rj_x0, rj_x0 + RJ_W, -1, W + 0.5, PCB_Z + PCB_T - 0.4, PCB_Z + PCB_T - 0.4 + RJ_H, op="cut", target=B)

# Auflagen unter der Platine (zwischen den Stiftleisten), dazu Schaumklebeband
pcb_xc = PCB_X0 + PCB_W / 2
for y in (30.0, 52.0):
    cyl(5, PCB_Z - W, [pcb_xc, y, W], "Z", "join", B)
for x0 in (PCB_X0 + 0.5, PCB_X0 + PCB_W - 3.5):
    box(x0, x0 + 3, W, W + 4, W, PCB_Z, op="join", target=B)

# Kabeldurchführungen oben: 12-24 V und RS485 A/B/GND
for x in (12.0, 32.0):
    cyl(7, W + 2, [x, H - W - 1, ZB / 2], "Y", "cut", B)

# Lüftungsschlitze in der Frontstufe
for z in (9.0, 16.0, 23.0):
    box(D + STEP_D - W - 1, D + STEP_D + 1, 32, 58, z, z + 1.6, op="cut", target=B)

# Dome für die Clip-Schrauben (M3 x 10 von der Schienenseite)
CLIP_SCREWS = [(RAIL_YC - 31, ZB / 2 + 1), (RAIL_YC + 31, ZB / 2 + 1)]
for y, z in CLIP_SCREWS:
    cyl(7, 5, [W, y, z], "X", "join", B)
    cyl(2.5, 8, [-1, y, z], "X", "cut", B)

# ===== Deckel ==============================================================
box(0, D, 0, H, ZB, ZB + LID, body=L)
box(D, D + STEP_D, STEP_Y0, STEP_Y1, ZB, ZB + LID, op="join", target=L)
for x, y in POSTS:
    cyl(3.4, LID + 2, [x, y, ZB - 1], "Z", "cut", L)
    cyl(6.2, 1.2, [x, y, ZB + LID - 1.2], "Z", "cut", L)
# Niederhalter auf der LAN-Buchse (0,4 mm Luft; mit Schaumband ausgleichen)
rj_top = PCB_Z + PCB_T + 13.5
box(rj_x0 + 1, rj_x0 + RJ_W - 1, W + 1, W + 15, rj_top + 0.4, ZB, op="join", target=L)

# ===== Clip (eigenes Teil, flach gedruckt) ==================================
rail_y0, rail_y1 = RAIL_YC - 17.5, RAIL_YC + 17.5
cz0, cz1 = 1.25, ZB + LID - 1.25
cx0, cx1 = -CLIP_T, 0.0
px0 = -CLIP_T - FL_T - CLR          # Rückseite des Hakens (hinter dem Schienenblech)
lx0 = px0 - 1.5                     # Hakenspitze
box(cx0, cx1, RAIL_YC - 35, RAIL_YC + 35, cz0, cz1, body=K)
# fester Haken oben
box(px0, cx0, rail_y1 + 0.1, rail_y1 + 3.5, cz0 + 2, cz1 - 2, op="join", target=K)
prism_xy([[px0, rail_y1 + 3.5], [lx0, rail_y1 + 3.5], [lx0, rail_y1 - 2], [px0, rail_y1 - 3]],
         cz0 + 2, cz1 - 2, "join", K)
# Fenster für den federnden Riegel unten
win_y0, win_y1 = rail_y0 - 6.5, rail_y0 + 0.8
wz0, wz1 = cz0 + 3, cz1 - 3
box(cx0 - 0.5, cx1 + 0.5, win_y0, win_y1, wz0, wz1, op="cut", target=K)
notch_z0 = wz1 - 5
box(cx0 - 0.5, cx1 + 0.5, RAIL_YC - 36, win_y1, notch_z0, wz1, op="cut", target=K)
# Federarm: an der linken Fensterkante angebunden, biegt sich in der Druckebene
arm_y1 = rail_y0 - 0.2
arm_y0 = arm_y1 - 1.6
box(cx0, cx1, arm_y0, arm_y1, wz0 - 0.5, wz1 - 1, op="join", target=K)
# Riegel mit Anlaufschräge am freien Ende
lz0, lz1 = wz1 - 8.5, wz1 - 1
box(px0, cx0, arm_y0, arm_y1, lz0, lz1, op="join", target=K)
prism_xy([[px0, arm_y0], [lx0, arm_y0], [lx0, rail_y0 - 0.5], [px0, rail_y0 + 3]],
         lz0, lz1, "join", K)
# Zuglasche nach unten (Schraubendreher durchs Loch, nach unten ziehen)
bz0, bz1 = notch_z0 + 1, wz1 - 1
box(cx0, cx1, -1, arm_y0 + 0.01, bz0, bz1, op="join", target=K)
box(lx0, cx1, -7, -1, bz0 - 3, bz1, op="join", target=K)
cyl(3.2, 8, [lx0 - 1, -4, (bz0 - 3 + bz1) / 2], "X", "cut", K)
# Freistich in der Gehäuserückwand, damit Federarm und Zuglasche frei gleiten
box(-1, 0.5, -1, win_y1 + 0.5, wz0 - 1, ZB + 1, op="cut", target=B)
# Schraublöcher
for y, z in CLIP_SCREWS:
    cyl(3.4, CLIP_T + 2, [cx0 - 1, y, z], "X", "cut", K)

# ===== Export ===============================================================
STL.mkdir(exist_ok=True)
c("file.save_mesh", path="stl/unterteil.stl", bodies=[B], quality="fine")
# Druckorientierung: Deckel mit der Außenseite aufs Bett, Clip mit der Gehäuseseite aufs Bett
c("solid.move", bodies=[L], axis=[1, 0, 0], angle="180 deg")
c("file.save_mesh", path="stl/deckel.stl", bodies=[L], quality="fine")
c("solid.move", bodies=[L], axis=[1, 0, 0], angle="-180 deg")
c("solid.move", bodies=[K], axis=[0, 1, 0], angle="90 deg")
c("file.save_mesh", path="stl/clip.stl", bodies=[K], quality="fine")
c("solid.move", bodies=[K], axis=[0, 1, 0], angle="-90 deg")

(HERE / "case.json").write_text(json.dumps({"commands": cmds}, indent=1), encoding="utf-8")
print(f"{len(cmds)} Befehle -> case.json")
