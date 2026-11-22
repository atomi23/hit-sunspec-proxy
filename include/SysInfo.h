#pragma once
#include <ArduinoJson.h>

// Hardware- und Softwaredaten des Geräts sowie CPU-Last für die Seite "Übersicht".
namespace SysInfo {
  void loop();
  void fill(JsonObject out);
}
