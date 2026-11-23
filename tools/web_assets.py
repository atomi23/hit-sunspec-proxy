"""Build the flash-resident web responses from their editable sources."""
import gzip


ASSETS = {
    "UI_CSS": "a.css",
    "UI_JS": "b.js",
    "DASH_HTML": "dashboard.html",
    "SYS_HTML": "system.html",
    "BLE_HTML": "bluetooth.html",
    "HISTORY_HTML": "history.html",
    "INDEX_HTML": "config.html",
    "SETUP_HTML": "setup.html",
    "DEBUG_HTML": "debug.html",
    "OTA_HTML": "ota.html",
}

CSS_LINK = b'<link rel="stylesheet" href="/a.css"/>'


def generate(source, header):
    css = (source / ASSETS["UI_CSS"]).read_bytes()
    lines = ["#pragma once", "#include <Arduino.h>", "", "namespace WebAssets {"]
    for name, filename in ASSETS.items():
        data = (source / filename).read_bytes()
        if filename.endswith(".html"):
            if data.count(CSS_LINK) != 1 or CSS_LINK not in data.split(b"</head>", 1)[0]:
                raise ValueError(f"{filename}: expected one shared stylesheet link in the head")
            data = data.replace(CSS_LINK, b"<style>" + css + b"</style>")
        compressed = gzip.compress(data, mtime=0)
        lines.append(f"inline constexpr uint8_t {name}[] PROGMEM = {{")
        for offset in range(0, len(compressed), 16):
            lines.append("  " + ", ".join(f"0x{byte:02x}" for byte in compressed[offset:offset + 16]) + ",")
        lines.append("};")
    lines += ["} // namespace WebAssets", ""]
    contents = "\n".join(lines).encode("ascii")
    if not header.exists() or header.read_bytes() != contents:
        header.parent.mkdir(parents=True, exist_ok=True)
        header.write_bytes(contents)
