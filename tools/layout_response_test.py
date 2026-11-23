#!/usr/bin/env python3
"""Verify the firmware's gzip responses, including the inlined stylesheet."""
import gzip
import io
from pathlib import Path
import re
import subprocess
import tempfile

from web_assets import ASSETS, generate

ROOT = Path(__file__).resolve().parents[1]
web = (ROOT / "src/WebConfig.cpp").read_text()
ota = (ROOT / "src/OtaUpdate.cpp").read_text()
https = (ROOT / "src/HttpsServer.cpp").read_text()
assert '"Content-Encoding"' in https.split("PASS_HEADERS[] = ", 1)[1].split(";", 1)[0]

program = r"""
#include <cassert>
#include <iostream>
#include <string>
#include "WebAssets.h"

struct Response {
  std::string type, encoding;
  const uint8_t *data;
  size_t size;
  void addHeader(const char *name, const char *value) {
    assert(std::string(name) == "Content-Encoding");
    encoding = value;
  }
};

struct AsyncWebServerRequest {
  Response response;
  Response *beginResponse(int status, const char *type, const uint8_t *data, size_t size) {
    assert(status == 200);
    response = {type, "", data, size};
    return &response;
  }
  void send(Response *r) {
    assert(r == &response);
    assert(r->encoding == "gzip");
    std::cout << r->type << '\n' << r->size << '\n';
    std::cout.write(reinterpret_cast<const char *>(r->data), r->size);
  }
};

namespace WebConfig {
"""
start = web.index("void sendGzip(")
program += web[start:web.index("\nvoid begin()", start)]
header = (ROOT / "include/WebConfig.h").read_text()
program += header[header.index("  template <size_t N>"):]
program += "\nint main() {\n  using namespace WebConfig;\n  AsyncWebServerRequest request;\n"

cases = []
for name, filename in ASSETS.items():
    data = (ROOT / "web" / filename).read_bytes()
    if filename.endswith(".html"):
        assert f"sendPage(req, WebAssets::{name});" in web + ota
        program += f"  sendPage(&request, WebAssets::{name});\n"
        tag = b'<link rel="stylesheet" href="/a.css"/>'
        assert 0 <= data.find(tag) < data.find(b"</head>")
        css = (ROOT / "web/a.css").read_bytes()
        data = data.replace(tag, b"<style>" + css + b"</style>", 1)
        mime = "text/html; charset=utf-8"
    else:
        mime = "text/css" if filename.endswith(".css") else "text/javascript"
        call = f'sendGzip(req, "{mime}", WebAssets::{name}, sizeof(WebAssets::{name}));'
        assert call in web
        program += "  " + call.replace("req,", "&request,") + "\n"
    cases.append((filename, mime, data))
program += "}\n"

with tempfile.TemporaryDirectory() as tmp:
    directory = Path(tmp)
    (directory / "Arduino.h").write_text("#include <cstdint>\n#define PROGMEM\n")
    generate(ROOT / "web", directory / "WebAssets.h")
    src = directory / "test.cpp"
    binary = src.with_suffix("")
    src.write_text(program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", tmp,
                    str(src), "-o", str(binary)], check=True)
    responses = io.BytesIO(subprocess.check_output([str(binary)]))
    for filename, mime, data in cases:
        assert responses.readline().decode("ascii").strip() == mime, filename
        size = int(responses.readline())
        payload = responses.read(size)
        assert len(payload) == size and gzip.decompress(payload) == data, filename
    assert not responses.read()
print("PASS: all eight pages include inline CSS; HTML, CSS and JS gzip responses round-trip byte for byte.")
