#pragma once
#include <Arduino.h>

// Systemprotokoll (RAM-Ring): Verbindungen, Fehler, Neustarts. Die §14a-Writes stehen getrennt in DebugLog.
struct SysEvt {
  uint32_t ms = 0;
  uint32_t epoch = 0; // 0 = Uhrzeit noch unbekannt
  char level = 'I';   // I, W, E
  char src[8] = "";
  char msg[96] = "";
};

namespace EventLog {
  void add(char level, const char *src, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
  void clear();
  size_t count();
  uint32_t warnings(); // W+E seit Start, auch wenn schon aus dem Ring gefallen
  String toJson(size_t maxN = 60);
}
