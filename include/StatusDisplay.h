#pragma once
#include "AppConfig.h"
#include "LiveData.h"

namespace StatusDisplay {
  void begin(const AppConfig &cfg);
  void loop();
  void setBrightness(uint8_t level);
  void applyConfig(const AppConfig &cfg);
  // Web-Live-Ansicht: Bildpuffer RGB565 (Byte-Reihenfolge wie am Panel: High-Byte zuerst), nullptr ohne Puffer
  const uint8_t *frame(size_t &len, int &w, int &h); // sperrt das Bild bis frameDone(), höchstens 5 s
  void frameDone();
  void requestTap(); // wie Antippen; wird im loop() ausgeführt
}
