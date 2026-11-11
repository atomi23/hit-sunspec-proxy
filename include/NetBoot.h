#pragma once
#include "AppConfig.h"

namespace NetBoot {
  void begin(const AppConfig &cfg);
  void loop(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg); // WLAN-Änderungen ohne Neustart übernehmen
}
