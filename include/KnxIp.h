#pragma once
#include "AppConfig.h"

namespace KnxIp {
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);
  void loop();
  void publishNow();
  bool ok();
}
