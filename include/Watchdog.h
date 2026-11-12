#pragma once
#include <Arduino.h>

namespace Watchdog {
  void begin(uint32_t timeoutSec = 45);
  void feed();
  // Soft: zu lange ohne gültige Datenquelle → geordneter Neustart
  void checkSoft(uint32_t maxNoSourceMs = 15UL * 60UL * 1000UL);
  // Grund des letzten Neustarts, Klartext für Web und Log
  const char *lastResetReason(bool en = false);
  // Welcher Schritt der Hauptschleife gerade läuft; überlebt den Neustart, damit ein Hänger zuzuordnen ist
  void stage(const char *name);
  // Schritt, in dem der letzte Watchdog-Neustart passierte, sonst leer
  const char *hungIn();
}
