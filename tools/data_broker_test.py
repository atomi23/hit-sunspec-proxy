#!/usr/bin/env python3
"""Compile the real DataBroker.cpp with host stubs and check source expiry."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

STUBS = {
    "Arduino.h": r"""
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
using std::max;
extern uint32_t nowMs;
inline uint32_t millis() { return nowMs; }
inline size_t strlcpy(char* dst, const char* src, size_t size) {
  if (size) { std::strncpy(dst, src, size); dst[size - 1] = '\0'; }
  return std::strlen(src);
}
""",
    "AppConfig.h": r"""
#pragma once
#include <cstdint>
struct Config {
  bool demoMode = false;
  uint32_t hitPollMs = 30000;
  bool mqttIngestEnabled = false;
  uint32_t mqttIngestMaxAgeMs = 120000;
  bool cloudEnabled = false;
  uint32_t cloudMaxAgeMs = 180000;
};
namespace ConfigStore { inline Config cfg; inline Config& get() { return cfg; } }
""",
    "LiveData.h": r"""
#pragma once
#include "DataSource.h"
struct LiveData {
  bool hitOk = false;
  uint32_t lastHitMs = 0;
  DataOrigin dataOrigin = DataOrigin::None;
  char status[48] = "boot";
};
inline LiveData gLive;
""",
    "MqttHa.h": r"""
#pragma once
#include <cstdint>
namespace MqttHa {
inline uint32_t lastMs = 0;
inline int applied = 0;
inline bool ingestFresh(uint32_t age) { return lastMs && millis() - lastMs <= age; }
inline void applyIngestToLive() { ++applied; }
}
""",
    "HoyCloud.h": r"""
#pragma once
#include <cstdint>
namespace HoyCloud {
inline uint32_t lastMs = 0;
inline bool fresh(uint32_t age) { return lastMs && millis() - lastMs <= age; }
}
""",
}

PROGRAM = r"""
#include "Arduino.h"
#include "AppConfig.h"
#include "LiveData.h"
#include "MqttHa.h"
#include "HoyCloud.h"
#include <iostream>
uint32_t nowMs = 1000000;
int failures = 0;
void check(bool ok, const char* name) {
  std::cout << (ok ? "PASS: " : "FAIL: ") << name << '\n';
  if (!ok) ++failures;
}
void origin(DataOrigin expected, const char* name) {
  DataBroker::reconcile();
  check(gLive.dataOrigin == expected, name);
}
int main() {
  auto& cfg = ConfigStore::get();
  origin(DataOrigin::None, "no sources -> None");
  check(std::strcmp(gLive.status, "keine Quelle") == 0, "no-source status");
  cfg.mqttIngestEnabled = cfg.cloudEnabled = true;
  MqttHa::lastMs = HoyCloud::lastMs = nowMs;
  gLive.hitOk = true;
  gLive.lastHitMs = nowMs;
  origin(DataOrigin::Native, "fresh HIT takes priority -> Native");
  check(MqttHa::applied == 0, "Native does not apply MQTT");
  gLive.hitOk = false;
  gLive.lastHitMs = nowMs - 90001;
  origin(DataOrigin::Mqtt, "stale HIT + fresh MQTT -> Mqtt");
  check(MqttHa::applied == 1 && std::strcmp(gLive.status, "MQTT Fallback") == 0,
        "MQTT applies data and sets fallback status");
  cfg.cloudEnabled = false;
  nowMs += cfg.mqttIngestMaxAgeMs + 1;
  origin(DataOrigin::None, "MQTT expires + cloud off -> None");
  check(std::strcmp(gLive.status, "keine Quelle") == 0, "expired MQTT clears fallback status");
  cfg.cloudEnabled = true;
  HoyCloud::lastMs = nowMs;
  origin(DataOrigin::Cloud, "stale HIT and MQTT + fresh cloud -> Cloud");
  check(std::strcmp(gLive.status, "Cloud Fallback") == 0, "cloud fallback status");
  nowMs += cfg.cloudMaxAgeMs + 1;
  origin(DataOrigin::None, "cloud expires -> None");
  check(std::strcmp(gLive.status, "keine Quelle") == 0, "expired cloud clears fallback status");
  cfg.demoMode = true;
  origin(DataOrigin::Demo, "demo enabled -> Demo");
  cfg.demoMode = false;
  origin(DataOrigin::None, "demo disabled without fresh sources -> None");
  gLive.hitOk = true;
  strlcpy(gLive.status, "HIT diagnostic", sizeof(gLive.status));
  origin(DataOrigin::None, "HIT marked OK but timestamp stale -> None");
  check(std::strcmp(gLive.status, "HIT diagnostic") == 0, "preserve HIT diagnostic status");
  std::cout << failures << " failure(s)\n";
  return failures ? 1 : 0;
}
"""

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    for name, contents in STUBS.items():
        (path / name).write_text(contents)
    (path / "check.cpp").write_text(PROGRAM)
    binary = path / "check"
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I", str(path), "-I", str(ROOT / "include"),
        str(ROOT / "src/DataBroker.cpp"), str(path / "check.cpp"),
        "-o", str(binary),
    ], check=True)
    raise SystemExit(subprocess.run([str(binary)]).returncode)
