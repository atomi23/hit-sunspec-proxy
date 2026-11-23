#pragma once
#include "AppConfig.h"
#include <Arduino.h>

namespace TasmotaClient {
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);
  void loop();
  void pollNow();

  void requestProbe(const AppConfig &cfg);
  bool takeProbeResult(String &raw, String &summary, bool &ok);
}
