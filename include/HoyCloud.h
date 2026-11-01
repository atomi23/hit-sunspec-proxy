#pragma once
#include "AppConfig.h"

namespace HoyCloud {
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);
  void loop();
  bool pollNow();
  bool fresh(uint32_t maxAgeMs);
}
