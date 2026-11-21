#!/usr/bin/env python3
"""Host regression checks for the firmware's actual chunk filler (requires g++)."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
source = (ROOT / "src/WebConfig.cpp").read_text()
formatters = source[source.index("size_t logLine("):source.index("template <typename Event")]
filler = source[source.index("[snapshot](uint8_t"):source.index("\n      });", source.index("[snapshot](uint8_t"))] + "}"
structs = "\n".join(
    (ROOT / f"include/{name}.h").read_text().split("struct ", 1)[1].split("};", 1)[0].join(["struct ", "};"])
    for name in ("EventLog", "DebugLog")
)
program = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
size_t strlcpy(char *out, const char *in, size_t n) {
  if (n) { std::strncpy(out, in, n); out[n-1] = 0; }
  return std::strlen(in);
}
''' + structs + formatters + r'''
template <typename Event> void check(size_t count) {
  struct Snapshot { Event events[80]; size_t count; };
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->count = count;
  std::string expected = "HIT-Proxy Protokoll (älteste Einträge zuerst; ms seit Start, ts Unixzeit oder 0)\n";
  for (size_t i = 0; i < count; i++) {
    snapshot->events[i].ms = i + 1;
    char line[256];
    expected.append(line, logLine(line, sizeof(line), snapshot->events[i]));
  }
  auto fill = ''' + filler + r''';
  for (size_t chunk : {1, 7, 64, 256, 4096}) {
    std::string actual;
    uint8_t buffer[4096], retry[4096];
    for (;;) {
      const size_t n = fill(buffer, chunk, actual.size());
      assert(n <= chunk);
      assert(fill(retry, chunk, actual.size()) == n);
      assert(std::memcmp(buffer, retry, n) == 0);
      if (!n) break;
      actual.append(reinterpret_cast<char *>(buffer), n);
    }
    assert(actual == expected);
    assert(fill(buffer, chunk, expected.size() + 100) == 0);
    assert(fill(buffer, 0, 0) == 0);
  }
}
int main() {
  check<SysEvt>(0); check<SysEvt>(1); check<SysEvt>(80);
  check<DebugEvt>(0); check<DebugEvt>(1); check<DebugEvt>(48);
  std::cout << "PASS: empty, single and full logs; chunk boundaries, retries and EOF\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory)
    (path / "check.cpp").write_text(program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    str(path / "check.cpp"), "-o", str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)

for kind in ("events", "debug"):
    route = source.split(f'server.on("/api/{kind}/download"', 1)[1].split("\n  });", 1)[0]
    assert route.index("if (!requireAuth(req)) return;") < route.index("downloadLog<")
    button = f"onclick=\"location.href='/api/{kind}/download'\""
    assert button in source
    assert button in (ROOT / "docs/ui-preview/debug.html").read_text()
assert 'req->send(503, "text/plain; charset=utf-8"' in source
print("PASS: authenticated routes, mirrored buttons and low-memory error response present")

history = (ROOT / "src/History.cpp").read_text()
constants = history[history.index("constexpr int SLOTS"):history.index("Store *st")]
snapshot = history[history.index("  struct Snapshot {", history.index("void downloadCsv")):history.index("  auto snap =", history.index("void downloadCsv"))]
key_formatter = history[history.index("void keyToStr("):history.index("void putBar(")]
csv_filler = history[history.index("[snap](uint8_t"):history.index("\n      });", history.index("[snap](uint8_t"))] + "}"
csv_program = program.split("struct SysEvt", 1)[0] + constants + key_formatter + r'''
using std::min;
int main() {
''' + snapshot + r'''
  auto snap = std::make_shared<Snapshot>();
  snap->pv = 100; snap->imp = 20; snap->exp = 30;
  auto fill = ''' + csv_filler + r''';
  auto render = [&](size_t chunk) {
    std::string result;
    uint8_t buffer[4096];
    for (;;) {
      size_t n = fill(buffer, chunk, result.size());
      assert(n <= chunk);
      if (!n) break;
      result.append(reinterpret_cast<char *>(buffer), n);
    }
    return result;
  };
  std::string empty = render(4096);
  assert(std::count(empty.begin(), empty.end(), '\n') == 2);
  assert(empty.find("Zaehler,Gesamt,,,,,100.000,20.000,30.000,,,") != std::string::npos);
  snap->nowSlot = 6000000;
  snap->haveRunning = true;
  snap->running = Sample{1, -2, 3, 4};
  snap->data.nDays = 1;
  snap->data.days[0] = Bar{20260101, 1, 2, 3, 4, 5, 6};
  snap->data.nMonths = 1;
  snap->data.months[0] = Bar{202601, 1, 2, 3, 4, 5, 6};
  snap->current = Bar{20260102, 10, 20, 30, 40, 50, 60};
  std::string data = render(4096);
  assert(std::count(data.begin(), data.end(), '\n') == 6);
  assert(data.find("Monat,2026-01,,,,,11.000,22.000,33.000,44.000,55.000,66.000") != std::string::npos);
  for (size_t chunk : {1, 7, 64, 256}) assert(render(chunk) == data);
  snap->current.key = 20260201;
  std::string extra = render(7);
  assert(std::count(extra.begin(), extra.end(), '\n') == 7);
  std::cout << "PASS: history CSV empty data, current slot/day/month, monthly merge and chunk boundaries\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory)
    (path / "check.cpp").write_text(csv_program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    str(path / "check.cpp"), "-o", str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)
route = source.split('server.on("/api/history/download"', 1)[1].split("\n  });", 1)[0]
assert route.index("if (!requireAuth(req)) return;") < route.index("History::downloadCsv(req)")
print("PASS: history CSV route requires authentication")
