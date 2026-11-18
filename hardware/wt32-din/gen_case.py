#!/usr/bin/env python3
"""Hutschienengehäuse (2 TE) für WT32-ETH01, ohne Schrauben.

- Hutschienen-Clip ist am Unterteil angedruckt: Haken oben fest, unten ein federnder Riegel.
  Die Feder ist eine senkrecht stehende Platte, die sich in der Druckebene biegt.
- Der Deckel rastet mit vier Federzungen in Fenster des Unterteils ein.
- Die Platine wird an den Enden geklemmt; darunter bleiben 13 mm frei (Stiftleisten, PoE-Adapter).

Erzeugt ein SolidCraft-Skript (case.json). Bauen und exportieren:

    python gen_case.py poe      # oder: rs485
    solidcraft-cli run case_poe.json --save wt32-din-poe.sc --script-relative

Koordinaten (mm): X = Tiefe (0 = Anlagefläche an der Hutschiene, + nach vorn), Y = Höhe (0 = unten),
Z = Breite (0 = Außenseite der Seitenwand des Unterteils, liegt beim Druck auf dem Bett).
Die LAN-Buchse zeigt nach oben. Alle Platinenmaße vor dem ersten Druck an der echten Platine prüfen.
"""
import json
import pathlib
import sys

# Varianten: "poe" nur LAN-/PoE-Platine, keine Kabeldurchführungen;
# "rs485" mit zwei Durchführungen unten für 12-24 V (Step-down) und RS485
VARIANTS = {"poe": {"cable_holes": False}, "rs485": {"cable_holes": True}}
VARIANT = sys.argv[1] if len(sys.argv) > 1 else "poe"
if VARIANT not in VARIANTS:
    sys.exit(f"Variante {VARIANT!r} unbekannt, erlaubt: {", ".join(VARIANTS)}")
OPT = VARIANTS[VARIANT]

HERE = pathlib.Path(__file__).resolve().parent

# --- Gehäuse ---------------------------------------------------------------
W = 2.0            # Wandstärke
D = 47.0           # Tiefe Grundkörper ab Schiene
H = 90.0           # Höhe (DIN 43880)
ZB = 33.0          # Höhe Unterteil
LID = 2.5          # Deckel -> Gesamtbreite 35,5 mm = 2 TE
STEP_D = 14.0      # Frontstufe (ragt durch den 45-mm-Ausschnitt der Verteilerabdeckung)
STEP_Y0, STEP_Y1 = 22.5, 67.5

# --- WT32-ETH01 ---------------------------------------------------------------
PX0 = 9.0          # Platinenkante hinten
PCB_W = 26.0
PCB_L = 57.0       # Platine ohne Überstand der Buchse
RJ_OVH = 3.0       # LAN-Buchse steht über die Platinenkante
RJ_FACE = H - 0.4  # Steckseite der Buchse, knapp hinter der Außenfläche
PCB_Y1 = RJ_FACE - RJ_OVH
PCB_Y0 = PCB_Y1 - PCB_L
PCB_Z = 15.0       # Unterseite Platine: 13 mm frei für Stiftleisten / PoE-Adapter
PCB_T = 1.6
RJ_W, RJ_HT = 18.0, 13.5   # Ausschnitt Breite, Höhe der Buchse über der Platine
HDR_H = 2.5        # Kunststoff der Stiftleisten auf der Oberseite

# --- Hutschiene TS35 ---------------------------------------------------------
RAIL_YC = 45.0
RY0, RY1 = RAIL_YC - 17.5, RAIL_YC + 17.5   # Unter-/Oberkante der Schiene
FL_T, CLR = 1.0, 0.3
LIP_F = -(FL_T + CLR)      # Vorderseite der Haken (hinter dem Schienenblech)
LIP_B = LIP_F - 1.7        # Rückseite der Haken

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
    """Polygon in X/Y, von z0 bis z1."""
    c("sketch.create", plane="XY", offset=z0)
    c("sketch.line", points=points, closed=True)
    c("sketch.finish")
    c("solid.extrude", distance=round(z1 - z0, 3), operation=op, targets=[target])


def prism_yz(points, x0, x1, op, target):
    """Polygon in Y/Z, von x0 bis x1."""
    c("sketch.create", plane="YZ", offset=x0)
    c("sketch.line", points=points, closed=True)
    c("sketch.finish")
    c("solid.extrude", distance=round(x1 - x0, 3), operation=op, targets=[target])


B = "Unterteil"
L = "Deckel"

# ===== Unterteil ===========================================================
box(0, D, 0, H, 0, ZB, body=B)
box(D, D + STEP_D, STEP_Y0, STEP_Y1, 0, ZB, op="join", target=B)
box(W, D - W, W, H - W, W, ZB + 1, op="cut", target=B)
box(D - W - 1, D + STEP_D - W, STEP_Y0 + W, STEP_Y1 - W, W, ZB + 1, op="cut", target=B)

# Lüftungsschlitze in der Frontstufe
for z in (9.0, 16.0, 23.0):
    box(D + STEP_D - W - 1, D + STEP_D + 1, 32, 58, z, z + 1.6, op="cut", target=B)

# LAN-Buchse durch die Oberseite; die Buchse liegt mit der Unterseite auf der Ausschnittkante
rj_x0 = PX0 + (PCB_W - RJ_W) / 2
box(rj_x0, rj_x0 + RJ_W, H - W - 1, H + 1, PCB_Z + PCB_T, PCB_Z + PCB_T + RJ_HT + 0.6, op="cut", target=B)

# Kabeldurchführungen unten: 12-24 V und RS485 (nur Variante rs485)
if OPT["cable_holes"]:
    for x in (23.0, 30.5):
        cyl(7, W + 2, [x, -1, 16.5], "Y", "cut", B)

# Platine: Stütze + Anschlag am Antennen-Ende, seitliche Führungen; die Unterseite bleibt frei
box(PX0 + 8, PX0 + 18, PCB_Y0 + 0.3, PCB_Y0 + 3.5, W, PCB_Z, op="join", target=B)
box(PX0 + 8, PX0 + 18, PCB_Y0 - 1.6, PCB_Y0 - 0.3, W, PCB_Z + PCB_T + 1.5, op="join", target=B)
for y0 in (PCB_Y0 + 12, PCB_Y0 + 38):
    box(PX0 - 2.1, PX0 - 0.3, y0, y0 + 10, W, PCB_Z + PCB_T + 1.5, op="join", target=B)
    box(PX0 + PCB_W + 0.3, PX0 + PCB_W + 2.1, y0, y0 + 10, W, PCB_Z + PCB_T + 1.5, op="join", target=B)

# ---- Hutschiene: fester Haken oben -----------------------------------------
hz0, hz1 = 1.0, ZB - 1.0
box(LIP_F, 0.01, RY1 + 0.1, RY1 + 3.5, hz0, hz1, op="join", target=B)
prism_xy([[LIP_F, RY1 + 3.5], [LIP_B, RY1 + 3.5], [LIP_B, RY1 - 2.0], [LIP_F, RY1 - 3.0]],
         hz0, hz1, "join", B)

# ---- federnder Riegel unten -------------------------------------------------
# Kanal frei schneiden (durch Seiten- und Rückwand), darin der bewegliche Teil mit Luft ringsum
SLOT_X1 = 32.0
SLOT_Y0, SLOT_Y1 = 19.0, RY0 - 0.1
box(-1, SLOT_X1, SLOT_Y0, SLOT_Y1, -1, ZB + 1, op="cut", target=B)
# Anker der Feder, steht auf der Seitenwand
box(SLOT_X1, SLOT_X1 + 4, SLOT_Y0 + 2.5, SLOT_Y1 - 1, W - 0.01, ZB - 0.6, op="join", target=B)

mz0, mz1 = 0.0, ZB - 0.6       # beweglicher Teil: auf dem Bett, 0,6 mm unter dem Deckel
BODY_Y0, BODY_Y1 = SLOT_Y0 + 3.8, RY0 - 0.5   # 3,8 mm Federweg nach unten
SPR_Y0, SPR_Y1 = BODY_Y0 + 0.2, BODY_Y0 + 2.0  # Federplatte 1,8 mm
# Federplatte (biegt sich in Y) und Riegelkörper
box(3.0, SLOT_X1 + 0.5, SPR_Y0, SPR_Y1, mz0, mz1, op="join", target=B)
box(LIP_B, 4.5, BODY_Y0, BODY_Y1, mz0, mz1, op="join", target=B)
# Riegelnase mit Anlaufschräge hinter der Unterkante der Schiene
prism_xy([[LIP_F, BODY_Y1 - 0.01], [LIP_F, RY0 + 3.0], [LIP_B, RY0 - 0.5], [LIP_B, BODY_Y1 - 0.01]],
         mz0, mz1, "join", B)
# Zugsteg hinter der Rückwand nach unten, Lasche mit Loch für den Schraubendreher
box(LIP_B, -0.6, -1.0, BODY_Y0 + 0.01, mz0, mz1, op="join", target=B)
box(LIP_B, 8.0, -7.0, -1.0, mz0, mz1, op="join", target=B)
cyl(3.2, 13, [LIP_B - 1, -4.0, (mz0 + mz1) / 2], "X", "cut", B)

# ---- Fenster für die Deckelzungen ------------------------------------------
FL, FT, FCL, BD = 10.0, 1.5, 0.2, 0.9   # Zungenlänge, -dicke, Luft zur Wand, Rasthöhe
zt = ZB - FL                             # Zungenspitze
TOP_TABS = [(3.5, 11.5), (34.0, 42.0)]
BOT_TABS = [(9.0, 17.0), (35.0, 43.0)]
for x0, x1 in TOP_TABS:
    box(x0 - 0.3, x1 + 0.3, H - W - 1, H + 1, zt - 0.3, zt + 2.5, op="cut", target=B)
for x0, x1 in BOT_TABS:
    box(x0 - 0.3, x1 + 0.3, -1, W + 1, zt - 0.3, zt + 2.5, op="cut", target=B)

# ===== Deckel ==============================================================
box(0, D, 0, H, ZB, ZB + LID, body=L)
box(D, D + STEP_D, STEP_Y0, STEP_Y1, ZB, ZB + LID, op="join", target=L)
# Federzungen mit Rastnase (Fangfläche oben, Anlaufschräge zur Spitze)
for x0, x1 in TOP_TABS:
    y1 = H - W - FCL
    box(x0, x1, y1 - FT, y1, zt, ZB + 0.01, op="join", target=L)
    prism_yz([[y1 - 0.01, zt], [y1 + BD, zt + 1.4], [y1 + BD, zt + 2.4], [y1 - 0.01, zt + 2.4]], x0, x1, "join", L)
for x0, x1 in BOT_TABS:
    y0 = W + FCL
    box(x0, x1, y0, y0 + FT, zt, ZB + 0.01, op="join", target=L)
    prism_yz([[y0 + 0.01, zt], [y0 - BD, zt + 1.4], [y0 - BD, zt + 2.4], [y0 + 0.01, zt + 2.4]], x0, x1, "join", L)
# Platine klemmen: Stempel am Antennen-Ende, Stege auf den Stiftleisten, Niederhalter auf der Buchse
pcb_top = PCB_Z + PCB_T
box(PX0 + 8, PX0 + 18, PCB_Y0 + 0.5, PCB_Y0 + 3.0, pcb_top, ZB + 0.01, op="join", target=L)
for x0 in (PX0 + 0.4, PX0 + PCB_W - 2.4):
    box(x0, x0 + 2.0, PCB_Y0 + 9, PCB_Y0 + 33, pcb_top + HDR_H + 0.2, ZB + 0.01, op="join", target=L)
box(rj_x0 + 1, rj_x0 + RJ_W - 1, RJ_FACE - 17, H - W - 0.2, pcb_top + RJ_HT + 0.2, ZB + 0.01, op="join", target=L)

# ===== Export ===============================================================
out = f"stl/{VARIANT}"
(HERE / out).mkdir(parents=True, exist_ok=True)
c("file.save_mesh", path=f"{out}/unterteil.stl", bodies=[B], quality="fine")
# Deckel mit der Außenseite aufs Bett
c("solid.move", bodies=[L], axis=[1, 0, 0], angle="180 deg")
c("file.save_mesh", path=f"{out}/deckel.stl", bodies=[L], quality="fine")
c("solid.move", bodies=[L], axis=[1, 0, 0], angle="-180 deg")

script = f"case_{VARIANT}.json"
(HERE / script).write_text(json.dumps({"commands": cmds}, indent=1), encoding="utf-8")
print(f"{VARIANT}: {len(cmds)} Befehle -> {script}")
