#!/usr/bin/env python3
"""Barlow (SIL OFL 1.1) als WOFF-Subset in die Firmware einbetten.

Schreibt include/UiFonts.h und docs/ui-preview/f/*.woff für die lokale Vorschau.
"""
import io
import pathlib
import urllib.request

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = pathlib.Path(__file__).resolve().parents[1]
CACHE = ROOT / "tools" / "fonts"
PREVIEW = ROOT / "docs" / "ui-preview" / "f"
SRC = "https://github.com/google/fonts/raw/main/ofl/barlow/Barlow-{}.ttf"
WEIGHTS = {400: "Regular", 600: "SemiBold"}
# Latin + Latin-1 (Umlaute, ß, °, ±, µ) + Typografie + Pfeile/Minus
UNICODES = list(range(0x20, 0x7F)) + list(range(0xA0, 0x100)) + [
    0x2009, 0x202F, 0x2013, 0x2014, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E,
    0x2026, 0x2032, 0x2190, 0x2191, 0x2192, 0x2193, 0x2212, 0x20AC,
]


def load(weight_name: str) -> bytes:
    CACHE.mkdir(parents=True, exist_ok=True)
    path = CACHE / f"Barlow-{weight_name}.ttf"
    if not path.exists():
        with urllib.request.urlopen(SRC.format(weight_name), timeout=30) as r:
            path.write_bytes(r.read())
    return path.read_bytes()


def make_woff(ttf: bytes) -> bytes:
    font = TTFont(io.BytesIO(ttf))
    opts = subset.Options()
    opts.layout_features = ["kern", "liga", "tnum", "lnum", "case"]
    opts.flavor = "woff"
    opts.name_IDs = ["*"]
    opts.notdef_outline = True
    s = subset.Subsetter(options=opts)
    s.populate(unicodes=UNICODES)
    s.subset(font)
    out = io.BytesIO()
    font.flavor = "woff"
    font.save(out)
    return out.getvalue()


def c_array(name: str, data: bytes) -> str:
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i + 20]) for i in range(0, len(data), 20)]
    body = ",\n  ".join(rows)
    return f"const uint8_t {name}[] PROGMEM = {{\n  {body}\n}};\nconst size_t {name}_LEN = {len(data)};\n"


def main():
    PREVIEW.mkdir(parents=True, exist_ok=True)
    parts = [
        "#pragma once\n// Generiert von tools/embed_fonts.py — nicht von Hand ändern.\n"
        "// Barlow © The Barlow Project Authors, SIL Open Font License 1.1\n"
        "#include <Arduino.h>\n\n"
    ]
    for weight, name in WEIGHTS.items():
        woff = make_woff(load(name))
        (PREVIEW / f"barlow-{weight}.woff").write_bytes(woff)
        parts.append(c_array(f"FONT_BARLOW_{weight}", woff))
        print(f"Barlow {weight}: {len(woff)} bytes")
    (ROOT / "include" / "UiFonts.h").write_text("\n".join(parts), encoding="utf-8")


if __name__ == "__main__":
    main()
