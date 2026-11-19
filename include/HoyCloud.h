#pragma once
#include "AppConfig.h"

// DigiCert Global Root G2 (+ Global Root CA), für Hoymiles und Dyness
extern const char CLOUD_CA_DIGICERT[];

namespace HoyCloud {
  void begin(const AppConfig &cfg); // startet die Abruf-Task
  void applyConfig(const AppConfig &cfg);
  bool pollNow();                   // nur aus der Abruf-Task
  bool fresh(uint32_t maxAgeMs);

  // Zugangsdaten prüfen, ohne sie zu speichern (Einrichtungsassistent). Läuft in der Abruf-Task.
  enum class Test : uint8_t { None = 0, Hoymiles = 1, Battery = 2 };
  void startTest(Test kind, const AppConfig &cfg);
  // 0 = kein Test, 1 = läuft, 2 = ok, 3 = fehlgeschlagen; msg ist der Text für die Oberfläche, station die gefundene Anlage
  uint8_t testState(String &msg, String &station);
}
