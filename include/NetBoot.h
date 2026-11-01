#pragma once
#include "AppConfig.h"

namespace NetBoot {
  void begin(const AppConfig &cfg);
  void loop(const AppConfig &cfg);
}
