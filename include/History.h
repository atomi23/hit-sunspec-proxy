#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Energieverlauf: 24 h in 5-Minuten-Mittelwerten, dazu Tages- und Monatssummen.
// Gespeichert in LittleFS (/hist.bin), Tagesgrenzen nach Ortszeit (NTP).
namespace History {
void begin();
void loop();
bool timeValid();
void day(JsonDocument &out);                          // 24-h-Kurven
void bars(const char *unit, int count, JsonDocument &out); // day | month | year | total
}
