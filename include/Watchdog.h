#pragma once
#include <Arduino.h>

namespace Watchdog {
  void begin(uint32_t timeoutSec = 45);
  void feed();
  // Soft: zu lange ohne gültige Datenquelle → geordneter Neustart
  void checkSoft(uint32_t maxNoSourceMs = 15UL * 60UL * 1000UL);
}
