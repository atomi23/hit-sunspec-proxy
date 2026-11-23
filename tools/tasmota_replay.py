#!/usr/bin/env python3
"""Replay the real Tasmota client, main loop and deferred web dispatch on a host."""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

STUBS = {
    "Arduino.h": r"""
#pragma once
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
using std::max;
using std::min;
struct String : std::string {
  using std::string::string;
  using std::string::operator=;
  String(std::string s) : std::string(std::move(s)) {}
  String() = default;
  String(const String &) = default;
  String(String &&) = default;
  String &operator=(const String &) = default;
  String &operator=(String &&) = default;
  void remove(size_t n) { erase(n); }
  size_t write(uint8_t c) { push_back(static_cast<char>(c)); return 1; }
  size_t write(const uint8_t *s, size_t n) { append(reinterpret_cast<const char *>(s), n); return n; }
};
inline String operator+(const std::string &s, unsigned n) { return s + std::to_string(n); }
inline size_t strlcpy(char *dst, const char *src, size_t n) {
  if (n) { strncpy(dst, src, n); dst[n - 1] = 0; }
  return strlen(src);
}
inline std::atomic<uint32_t> nowMs{1000};
inline uint32_t millis() { return nowMs; }
inline void delay(uint32_t ms) { nowMs += ms; }
""",
    "freertos/task.h": r"""
#pragma once
#include <freertos/FreeRTOS.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>
struct Task {
  std::mutex lock;
  std::condition_variable cv;
  bool notified = false, waiting = false, inGet = false, releaseGet = false;
};
using TaskHandle_t = Task *;
inline Task *replayTask = nullptr;
inline thread_local Task *currentTask = nullptr;
constexpr int pdPASS = 1;
template <typename Predicate>
void waitFor(Task &t, std::unique_lock<std::mutex> &lock, Predicate pred) {
  if (!t.cv.wait_for(lock, std::chrono::seconds(5), pred)) std::abort();
}
inline int xTaskCreatePinnedToCore(void (*fn)(void *), const char *, unsigned, void *arg,
                                   int, TaskHandle_t *handle, int) {
  *handle = replayTask = new Task;
  std::thread([=, task = *handle] { currentTask = task; fn(arg); }).detach();
  return pdPASS;
}
inline void ulTaskNotifyTake(int, TickType_t) {
  auto &t = *currentTask;
  std::unique_lock<std::mutex> lock(t.lock);
  t.waiting = true;
  t.cv.notify_all();
  waitFor(t, lock, [&] { return t.notified; });
  t.waiting = t.notified = false;
}
inline void xTaskNotifyGive(TaskHandle_t t) {
  std::lock_guard<std::mutex> lock(t->lock);
  t->notified = true;
  t->cv.notify_all();
}
""",
    "HTTPClient.h": r"""
#pragma once
#include <Arduino.h>
#include <freertos/task.h>
#include <cassert>
inline bool silentMeter = false;
inline unsigned requests = 0;
inline String lastUrl;
constexpr uint32_t HTTP_TICK_MS = 10;
constexpr char METER_JSON[] = R"({"StatusSNS":{"ENERGY":{"Power":-321,"Total":123.25,"Export":67.5,"Meter_Number":1234}}})";
struct HTTPClient {
  unsigned timeout = 0;
  void setConnectTimeout(unsigned ms) { assert(ms == 2000); }
  void setTimeout(unsigned ms) { timeout = ms; }
  bool begin(const String &url) { ++requests; lastUrl = url; return true; }
  int GET() {
    assert(timeout == 3000);
    if (currentTask) {
      auto &t = *currentTask;
      std::unique_lock<std::mutex> lock(t.lock);
      t.inGet = true;
      t.cv.notify_all();
      waitFor(t, lock, [&] { return t.releaseGet; });
      t.inGet = t.releaseGet = false;
    } else if (silentMeter) {
      // Arduino checks elapsed > timeout, waiting 10 ms between checks.
      delay(timeout + HTTP_TICK_MS);
    }
    return silentMeter ? -11 : 200;
  }
  String getString() { return METER_JSON; }
  void end() {}
};
""",
}

PROGRAM = r"""
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <freertos/semphr.h>
#include "LiveData.h"
#include "TasmotaClient.h"

constexpr uint32_t LOOP_TICK_MS = 2;
struct Service {
  uint32_t prev = 0, maxDelay = 0;
  void tick() {
    maxDelay = max(maxDelay, millis() - prev - LOOP_TICK_MS);
    prev = millis();
  }
};
Service sunspec, gateway;
AppConfig config;
unsigned saves = 0;
namespace ConfigStore {
AppConfig &get() { return config; }
bool save() { ++saves; return true; }
}
namespace Watchdog {
void feed() {}
void stage(const char *) {}
void checkSoft() {}
}
namespace NetBoot { void loop(const AppConfig &) {} void applyConfig(const AppConfig &) {} }
namespace HitClient { void loop() {} void pollNow() {} }
namespace MqttHa { void loop() {} void publishNow() {} }
namespace DataBroker { void reconcile() {} }
namespace History { void loop() {} }
namespace KnxIp { void loop() {} }
namespace SunSpecServer { void loop() { sunspec.tick(); } void rebuildMap() {} }
namespace ModbusGateway { void loop() { gateway.tick(); } }
namespace StatusDisplay { void loop() {} void applyConfig(const AppConfig &) {} }
namespace HitControl { bool applyAll() { return false; } String lastError() { return ""; } }
namespace SysInfo { void loop() {} }
namespace OtaUpdate { void loop() {} }
namespace HttpsServer { void loop() {} }

namespace WebConfig {
void runPendingApply() {}
#include "web_dispatch.inc"
}
#include "main_loop.inc"

#if ASYNC
void finishHttp(bool silent, bool disableInFlight = false) {
  auto &task = *replayTask;
  {
    std::unique_lock<std::mutex> lock(task.lock);
    waitFor(task, lock, [&] { return task.inGet; });
  }
  if (disableInFlight) {
    assert(gLive.tasmotaOk);
    config.tasmotaEnabled = false;
    TasmotaClient::applyConfig(config);
    assert(!gLive.tasmotaOk);
  }
  if (silent) {
    const auto started = millis();
    while (millis() - started < 3000 + HTTP_TICK_MS) loop();
  }
  std::unique_lock<std::mutex> lock(task.lock);
  task.releaseGet = true;
  task.cv.notify_all();
  waitFor(task, lock, [&] { return task.waiting; });
}
#endif

int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  const bool probe = scenario.find("probe") != std::string::npos;
  const bool disableInFlight = scenario == "disable in-flight silent";
  const bool manual = scenario == "manual silent" || disableInFlight;
  const bool disabled = scenario == "poll disabled";
  const bool silent = scenario.find("silent") != std::string::npos;
  silentMeter = silent && !manual;
  config.tasmotaEnabled = !disabled && !probe;
  config.uiLang = UiLang::En;
  TasmotaClient::begin(config);
  if (probe) {
    auto &req = WebConfig::probeReq;
    req = config;
    req.tasmotaEnabled = true;
    strlcpy(req.tasmotaHost, "198.51.100.42", sizeof(req.tasmotaHost));
    strlcpy(req.tasmotaUser, "a+b", sizeof(req.tasmotaUser));
    strlcpy(req.tasmotaPass, "x&y", sizeof(req.tasmotaPass));
#if ASYNC
    WebConfig::probeLock = xSemaphoreCreateMutex();
    WebConfig::probeState = WebConfig::ProbeState::Queued;
#else
    WebConfig::probePending = true;
    WebConfig::addWork(WebConfig::W_PROBE);
#endif
  }
  sunspec.prev = gateway.prev = millis() - LOOP_TICK_MS;
  loop();
  if (manual) {
#if ASYNC
    finishHttp(false);
    loop();
#endif
    assert(requests == 1 && gLive.tasmotaOk);
    silentMeter = true;
    WebConfig::addWork(WebConfig::W_POLL);
    loop();
  }
#if ASYNC
  if (probe || manual) loop(); // The queued job starts at the next Tasmota stage.
  if (!disabled) {
    assert(gLive.tasmotaOk == manual && saves == 0);
    finishHttp(silent, disableInFlight);
  }
  loop();
#else
  if (!probe || silent) loop();
#endif
  const bool expectValues = !silent && !disabled;
  assert(requests == (manual ? 2u : disabled ? 0u : 1u));
  assert(gLive.tasmotaOk == expectValues);
  if (expectValues) {
    assert(gLive.tasmotaPowerW == -321 && gLive.tasmotaTotalKwh == 123.25 &&
           gLive.tasmotaExportKwh == 67.5 && gLive.lastTasmotaMs != 0);
    assert(!strcmp(gLive.tasmotaSensorKey, "ENERGY") && !strcmp(gLive.tasmotaMeterId, "1234"));
  }
  if (probe) {
    assert(requests == 1);
    assert(lastUrl == "http://198.51.100.42:80/cm?user=a%2Bb&password=x%26y&cmnd=Status%208");
    const auto saved = expectValues && config.tasmotaEnabled &&
                       !strcmp(config.tasmotaHost, "198.51.100.42");
    assert(saves == unsigned(expectValues) && saved == expectValues);
#if ASYNC
    assert(WebConfig::probeState == WebConfig::ProbeState::Done);
    assert(WebConfig::probeResult.find(expectValues ? "Power -321 W" : "HTTP request failed") != String::npos);
#else
    assert(WebConfig::probeDone && !WebConfig::probePending && WebConfig::probeOk == expectValues);
    assert(strstr(WebConfig::probeSummary, expectValues ? "Power -321 W" : "HTTP request failed"));
#endif
  }
  const auto expectedDelay = !ASYNC && silent ? 3000 + HTTP_TICK_MS : 0;
  assert(sunspec.maxDelay == expectedDelay && gateway.maxDelay == expectedDelay);
  printf("%-12s SunSpec +%4u ms  Gateway +%4u ms  meter %s\n",
         scenario.c_str(), sunspec.maxDelay, gateway.maxDelay,
         expectValues ? "-321 W / 123.25 in / 67.50 out kWh" :
         disableInFlight ? "invalid at disable and after timeout" : disabled ? "disabled" : "timeout");
}
"""


def between(source, start, end):
    i = source.index(start)
    return source[i:source.index(end, i + len(start))]


def replay(tree, json_include, tmp, baseline=False):
    web = (tree / "src/WebConfig.cpp").read_text()
    main = (tree / "src/main.cpp").read_text()
    dispatch = (between(web, "volatile uint8_t applyPending", "void scheduleApply")
                + between(web, "void runDeferredWork()", "void jsonEscapeAppend")
                + between(web, "void loop() {\n  runPendingApply();", "} // namespace WebConfig"))
    (tmp / "web_dispatch.inc").write_text(dispatch)
    (tmp / "main_loop.inc").write_text(main[main.index("void loop() {"):])
    exe = tmp / "replay"
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pthread",
                    f"-DASYNC={int(not baseline)}", f"-I{tmp}", f"-I{ROOT / 'tools/host_stubs'}",
                    f"-I{tree / 'include'}", f"-I{json_include}", str(tmp / "replay.cpp"),
                    str(tree / "src/TasmotaClient.cpp"), "-o", str(exe)], check=True)
    print(f"{'BEFORE' if baseline else 'AFTER'}: worst extra service gap over the 2 ms loop tick", flush=True)
    for scenario in ("poll silent", "manual silent", "probe silent", "poll normal", "probe normal", "poll disabled"):
        subprocess.run([str(exe), scenario], check=True, timeout=10)
    if not baseline:
        subprocess.run([str(exe), "disable in-flight silent"], check=True, timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, help="Optional checkout of pre-worker firmware")
    parser.add_argument("--arduino-json", type=Path,
                        default=ROOT / ".pio/libdeps/t-connect-pro/ArduinoJson/src")
    args = parser.parse_args()
    if not (args.arduino_json / "ArduinoJson.h").is_file():
        parser.error("ArduinoJson missing; run pio run -e t-connect-pro or specify --arduino-json")
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        for name, contents in STUBS.items():
            path = tmp / name
            path.parent.mkdir(exist_ok=True)
            path.write_text(contents)
        (tmp / "replay.cpp").write_text(PROGRAM)
        if args.baseline:
            replay(args.baseline.resolve(), args.arduino_json, tmp, baseline=True)
        replay(ROOT, args.arduino_json, tmp)


if __name__ == "__main__":
    main()
