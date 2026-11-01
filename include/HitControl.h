#pragma once
#include "AppConfig.h"
#include <Arduino.h>

namespace HitControl {
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);

  // Schreibt aktuelle Config-Limits an HIT (nur wenn hitWriteEnabled)
  bool applyBatteryLimits();
  bool applyApcLimit();
  bool applyAll();

  // Von SunSpec Model 123 (Consolinno) — pctSF2: 10000=100.00%
  bool onSunSpecPowerLimit(uint16_t pctSF2, bool enabled);

  String lastError();
}
