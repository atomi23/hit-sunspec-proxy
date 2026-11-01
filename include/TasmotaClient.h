#pragma once
#include "AppConfig.h"
#include <Arduino.h>

namespace TasmotaClient {
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);
  void loop();
  bool pollNow();

  // One-Click: Status 8 holen, parsen, Roh-JSON + erkannte Felder liefern
  bool probe(String &rawOut, String &summaryOut);
}
