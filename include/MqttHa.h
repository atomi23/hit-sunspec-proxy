#pragma once
#include "AppConfig.h"

namespace MqttHa {
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);
  void loop();
  void publishNow();
}
