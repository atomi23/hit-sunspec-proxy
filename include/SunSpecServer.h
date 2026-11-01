#pragma once
#include "AppConfig.h"
#include "LiveData.h"

namespace SunSpecServer {
  void begin(const AppConfig &cfg);
  void loop();
  void rebuildMap();           // fill holding regs from gLive
  void applyConfig(const AppConfig &cfg);
}
