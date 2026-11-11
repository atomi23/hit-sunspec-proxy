#!/usr/bin/env python3
"""Barlow als kantengeglättete VLW-Schriften (LovyanGFX) für das Board-Display.

Schreibt include/DispFonts.h. Benötigt Pillow; TTFs kommen aus tools/fonts (embed_fonts.py lädt sie).
"""
import pathlib
import struct

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[1]
FONTS = ROOT / "tools" / "fonts"

LATIN = "".join(chr(c) for c in range(0x20, 0x7F)) + "".join(chr(c) for c in range(0xA0, 0x100)) + "–—…→←↑↓"
NUMERIC = " 0123456789.,-–+%WkVAhHz"

# name, Datei, Pixelgröße, Zeichenvorrat
SPECS = [
    ("DFONT_HERO", "Barlow-SemiBold.ttf", 58, NUMERIC),
    ("DFONT_VAL", "Barlow-SemiBold.ttf", 26, LATIN),
    ("DFONT_LBL", "Barlow-Regular.ttf", 17, LATIN),
    ("DFONT_SM", "Barlow-Regular.ttf", 14, LATIN),
]


def build_vlw(ttf: pathlib.Path, size: int, chars: str) -> bytes:
    font = ImageFont.truetype(str(ttf), size)
    ascent, descent = font.getmetrics()
    glyphs = []
    for ch in sorted(set(chars), key=ord):
        adv = int(round(font.getlength(ch)))
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = max(0, x1 - x0), max(0, y1 - y0)
        if ch == " " or w == 0 or h == 0:
            glyphs.append((ord(ch), 0, 0, adv, 0, 0, b""))
            continue
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
        glyphs.append((ord(ch), h, w, adv, -y0, x0, img.tobytes()))
    out = bytearray(struct.pack(">6i", len(glyphs), 11, size, 0, ascent, descent))
    for code, h, w, adv, dy, dx, _ in glyphs:
        out += struct.pack(">7i", code, h, w, adv, dy, dx, 0)
    for *_, bmp in glyphs:
        out += bmp
    return bytes(out)


def c_array(name: str, data: bytes) -> str:
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i + 24]) for i in range(0, len(data), 24)]
    return f"const uint8_t {name}[] PROGMEM = {{\n  " + ",\n  ".join(rows) + "\n};\n"


def main():
    parts = [
        "#pragma once\n// Generiert von tools/make_vlw.py — nicht von Hand ändern.\n"
        "// Barlow © The Barlow Project Authors, SIL Open Font License 1.1\n"
        "#include <Arduino.h>\n\n"
    ]
    for name, file, size, chars in SPECS:
        data = build_vlw(FONTS / file, size, chars)
        parts.append(c_array(name, data))
        print(f"{name}: {size}px, {len(set(chars))} Zeichen, {len(data)} Bytes")
    (ROOT / "include" / "DispFonts.h").write_text("\n".join(parts), encoding="utf-8")


if __name__ == "__main__":
    main()
