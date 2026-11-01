#pragma once
#include "AppConfig.h"
#include "LiveData.h"

namespace StatusDisplay {
  void begin(const AppConfig &cfg);
  void loop();
  void setBrightness(uint8_t level);
  void applyConfig(const AppConfig &cfg);
}
