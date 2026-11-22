#pragma once
#include "AppConfig.h"
#include <ArduinoJson.h>

// Bluetooth-LED-Strip (App "Lotus Lantern") schalten; nur auf Boards mit Bluetooth im ESP.
namespace BleLed {
#ifdef HAS_BLE_LED
constexpr bool SUPPORTED = true;
#else
constexpr bool SUPPORTED = false;
#endif
  void begin(const AppConfig &cfg);
  void applyConfig(const AppConfig &cfg);
  void loop();
  void setPower(bool on);
  void setBrightness(uint8_t pct);            // 0 schaltet aus
  void setColor(uint8_t r, uint8_t g, uint8_t b); // 0/0/0 schaltet aus
  void startScan();
  void status(JsonObject out);
}
