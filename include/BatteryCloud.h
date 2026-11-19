#pragma once
#include <Arduino.h>
#include "AppConfig.h"
#include "BatteryData.h"

// Akku-Clouds: ein Treiber je Hersteller, ausgewählt über cfg.batCloud.
// Läuft in der Abruf-Task der Hoymiles-Cloud, damit nie zwei TLS-Sitzungen gleichzeitig offen sind.

// Data: neue Werte, dann das Intervall warten; DataMore: neue Werte, gleich die nächste Anfrage (erste Runde)
// NoLink: die Cloud wurde gar nicht erreicht, zählt also nicht gegen ihre Abrufgrenze; bald erneut versuchen
enum class CloudResult : int8_t { NoLink = -2, Error = -1, Busy = 0, Data = 1, RateLimited = 2, DataMore = 3 };

struct BatteryCloudDriver {
  virtual ~BatteryCloudDriver() = default;
  // Genau eine Anfrage. io behält die Werte zwischen den Aufrufen.
  // Busy: Anfrage ok, aber noch Einrichtung (Gerät suchen, binden); dann kurz darauf erneut.
  virtual CloudResult poll(const AppConfig &cfg, BatteryCloudData &io, String &err) = 0;
  virtual void reset() = 0;
};

BatteryCloudDriver &dynessDriver();
bool dynessSignSelftest(); // fester Testvektor, siehe BatteryDyness.cpp

namespace BatteryCloud {
void step(const AppConfig &cfg); // aus der Cloud-Task, alle 250 ms
void reset();                    // neue Einstellungen
}
