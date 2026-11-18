#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "BatteryData.h"

// Reine Logik ohne Netzwerk: Registerdekodierung, Dyness-Punkte, Zusammenführen

// regs = Input-Register 1916..1927. false: alle zwölf 0, das BMS liefert nichts.
bool decodeHitBattery(const uint16_t regs[12], BatteryHit &out);
// regs = Holding-Register 8100+50*i .. +27
void decodeHitPack(const uint16_t regs[28], BatteryPack &out);

// Dyness Open API: zu signierender Text
String dynessStringToSign(const char *method, const char *contentMd5, const char *date, const char *path);
// data[] aus /v1/device/realTime/data ({pointId, pointValue}); keepCells=false speichert keine Einzelzellen
void dynessParsePoints(JsonArrayConst points, BatteryCloudData &io, bool keepCells);

// Ein Wert je Größe für Live-Kachel, Home Assistant und KNX
struct BatteryMerged {
  float cellTempMax = NAN, cellTempMin = NAN, modTempMax = NAN, modTempMin = NAN;
  float cellVMax = NAN, cellVMin = NAN, cellDiffMv = NAN;
  float chargeLimA = NAN, dischargeLimA = NAN;
  float cycles = NAN, remainKwh = NAN;
  int8_t alarm = -1;
  const char *srcCellV = nullptr, *srcTemp = nullptr, *srcLim = nullptr;
};
BatteryMerged mergeBattery(const BatteryInfo &b, uint32_t nowMs, uint32_t cloudIntervalMs);
