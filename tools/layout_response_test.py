#!/usr/bin/env python3
"""Verify that the firmware streams styled HTML without a separate CSS request."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
web = (ROOT / "src/WebConfig.cpp").read_text()
ota = (ROOT / "src/OtaUpdate.cpp").read_text()


def literal(source, name):
    return re.search(rf'const char {name}\[\] PROGMEM = R"(\w+)\(.*?\)\1";', source, re.S)[0]


pages = ("DASH_HTML", "HISTORY_HTML", "INDEX_HTML", "SETUP_HTML", "DEBUG_HTML", "OTA_HTML")
for page in pages:
    assert f"sendPage(req, {page});" in web + ota

program = r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
using std::min;
#define PROGMEM

struct AsyncWebServerRequest {
  size_t total = 0;
  std::function<size_t(uint8_t*, size_t, size_t)> fill;
  std::string fallback;
  void send(const char*, size_t length, decltype(fill) callback) {
    total = length;
    fill = callback;
  }
  void send(int status, const char*, const uint8_t* data, size_t length) {
    assert(status == 200);
    fallback.assign(reinterpret_cast<const char*>(data), length);
  }
};
"""
program += "\n".join(literal(ota if page == "OTA_HTML" else web, page) for page in (*pages, "UI_CSS"))
start = web.index("void sendPage(")
program += "\n" + web[start:web.index("\nvoid begin()", start)]
program += r"""
int main() {
  const std::string tag = "<link rel=\"stylesheet\" href=\"/a.css\"/>";
  for (const char* page : {DASH_HTML, HISTORY_HTML, INDEX_HTML, SETUP_HTML, DEBUG_HTML, OTA_HTML}) {
    std::string expected(page);
    const size_t pos = expected.find(tag);
    assert(pos < expected.find("</head>"));
    expected.replace(pos, tag.size(), std::string("<style>") + UI_CSS + "</style>");
    AsyncWebServerRequest req;
    sendPage(&req, page);
    assert(req.total == expected.size());
    uint8_t buffer[1460];
    for (size_t size : {1, 17, 1460}) {
      for (size_t i = 0; i < req.total; i += size) {
        const size_t n = min(req.total - i, size);
        for (int retry = 0; retry < 2; ++retry) {
          assert(req.fill(buffer, size, i) == n);
          assert(memcmp(buffer, expected.data() + i, n) == 0);
        }
      }
    }
    assert(req.fill(buffer, sizeof(buffer), req.total) == 0);
  }
  AsyncWebServerRequest req;
  sendPage(&req, "<p>No style link</p>");
  assert(req.fallback == "<p>No style link</p>");
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "test.cpp"
    binary = src.with_suffix("")
    src.write_text(program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(src), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("All six pages stream the shared stylesheet inline, including across retries and chunk boundaries.")
