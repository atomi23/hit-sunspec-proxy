#!/usr/bin/env python3
"""Host regression checks for the firmware's actual chunk filler (requires g++)."""
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
source = (ROOT / "src/WebConfig.cpp").read_text()
formatters = source[source.index("size_t logLine("):source.index("template <typename Event")]
filler_start = source.index("[snapshot, en](uint8_t")
filler = source[filler_start:source.index("\n      });", filler_start)] + "}"
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
template <typename Event> void check(size_t count, bool en) {
  struct Snapshot { Event events[80]; size_t count; };
  auto snapshot = std::make_shared<Snapshot>();
  snapshot->count = count;
  std::string expected = en ? "HIT-Proxy log (oldest entries first; ms since startup, ts Unix time or 0)\n"
                            : "HIT-Proxy Protokoll (älteste Einträge zuerst; ms seit Start, ts Unixzeit oder 0)\n";
  for (size_t i = 0; i < count; i++) {
    snapshot->events[i].ms = i + 1;
    char line[256];
    expected.append(line, logLine(line, sizeof(line), snapshot->events[i], en));
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
  for (bool en : {false, true}) {
    check<SysEvt>(0, en); check<SysEvt>(1, en); check<SysEvt>(80, en);
    check<DebugEvt>(0, en); check<DebugEvt>(1, en); check<DebugEvt>(48, en);
    DebugEvt event{};
    char line[128];
    logLine(line, sizeof(line), event, en);
    assert(std::strstr(line, en ? " Value=" : " Wert="));
  }
  std::cout << "PASS: both languages, empty/single/full logs, chunk boundaries, retries and EOF\n";
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

chunk_program = r'''
#include "HttpChunked.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
bool decode(const std::string &wire, std::string &out, size_t fragment = 4096,
            bool writeOk = true) {
  size_t position = 0;
  char buffer[13];
  return decodeHttpChunks([&](uint8_t *data, size_t size) -> int {
    size_t n = std::min({size, fragment, wire.size() - position});
    std::memcpy(data, wire.data() + position, n);
    position += n;
    return int(n);
  }, [&](const char *data, size_t size) {
    assert(size <= sizeof(buffer));
    out.append(data, size);
    return writeOk;
  }, buffer, sizeof(buffer));
}
int main() {
  for (size_t fragment : {1, 2, 7, 4096}) {
    std::string out;
    assert(decode("4;name=value\r\nWiki\r\n5\r\npedia\r\n0\r\nX-Trailer: ignored\r\n\r\n", out, fragment));
    assert(out == "Wikipedia");
    out.clear();
    assert(decode("0\r\n\r\n", out, fragment));
    assert(out.empty());
    out.clear();
    std::string payload(4096, '\0');
    assert(decode("1000\r\n" + payload + "\r\n0\r\n\r\n", out, fragment));
    assert(out == payload);
    out.clear();
    assert(decode("A\r\n0123456789\r\n0;end=yes\r\n\r\n", out, fragment));
    assert(out == "0123456789");
  }
  for (const char *bad : {"", "\r\n", "z\r\n", "1\r\n", "2\r\na",
       "1\r\naX\r\n0\r\n\r\n", "0\r\n", "0\r\nTrailer: truncated",
       "1\na\r\n0\r\n\r\n", "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\r\n"}) {
    std::string out;
    assert(!decode(bad, out));
  }
  std::string out;
  assert(!decode("1\r\na\r\n0\r\n\r\n", out, 1, false));
  std::cout << "PASS: HTTPS de-chunking, extensions, trailers, fragmented/binary payloads and malformed/truncated/error paths\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = pathlib.Path(directory)
    (path / "check.cpp").write_text(chunk_program)
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT / "include"), str(path / "check.cpp"),
                    "-o", str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)
