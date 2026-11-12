#pragma once
#include "AppConfig.h"

namespace HoyCloud {
  void begin(const AppConfig &cfg); // startet die Abruf-Task
  void applyConfig(const AppConfig &cfg);
  bool pollNow();                   // nur aus der Abruf-Task
  bool fresh(uint32_t maxAgeMs);
}
