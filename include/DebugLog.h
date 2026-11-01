#pragma once
#include <Arduino.h>

struct DebugEvt {
  uint32_t ms = 0;
  uint8_t fc = 0;
  uint16_t addr = 0;
  uint16_t value = 0;
  char note[40] = "";
};

namespace DebugLog {
  void clear();
  void add(uint8_t fc, uint16_t addr, uint16_t value, const char *note);
  size_t count();
  const DebugEvt *at(size_t i); // 0 = oldest in window
  // JSON array string (ring, newest last)
  String toJson(size_t maxN = 40);
  uint32_t lastWriteMs();
  uint16_t lastAddr();
  uint16_t lastValue();
  const char *lastNote();
}
