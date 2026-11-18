#pragma once
#include "AppConfig.h"

// DigiCert Global Root G2 (+ Global Root CA), für Hoymiles und Dyness
extern const char CLOUD_CA_DIGICERT[];

namespace HoyCloud {
  void begin(const AppConfig &cfg); // startet die Abruf-Task
  void applyConfig(const AppConfig &cfg);
  bool pollNow();                   // nur aus der Abruf-Task
  bool fresh(uint32_t maxAgeMs);
}
