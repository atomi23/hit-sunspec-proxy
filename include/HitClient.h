#pragma once
#include "AppConfig.h"
#include "LiveData.h"

namespace HitClient {
  void begin(const AppConfig &cfg);
  void loop();                 // non-blocking poll scheduler
  bool pollNow();              // force one poll
  void applyConfig(const AppConfig &cfg);
}
