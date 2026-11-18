#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "AppConfig.h"

// Zusätzliche Akkuwerte, getrennt von gLive: SunSpec, History und der Datenquellen-Wechsel sehen sie nie.
// Fehlende Werte sind NAN.

constexpr uint8_t BAT_MAX_PACKS = 4;
constexpr uint8_t BAT_MAX_CELLS = 16;

// HIT, Input-Register 1916-1927 (V2.8, Kap. 4.2.3)
struct BatteryHit {
  bool valid = false;
  uint32_t atMs = 0;
  float maxChargeA = NAN, maxDischargeA = NAN;
  float chargeCutV = NAN, dischargeCutV = NAN;
  float modTempMax = NAN, modTempMin = NAN;
  float cellTempMax = NAN, cellTempMin = NAN;
  float modVMax = NAN, modVMin = NAN;
  float cellVMax = NAN, cellVMin = NAN;
};

// HIT, Holding-Register je Pack ab 8100 (50er-Schritte)
struct BatteryPack {
  char sn[33] = "";
  uint32_t ratedCap = 0; // Rohwert, Einheit im Protokoll nicht angegeben
  uint32_t bmsFw = 0;
  uint16_t dcdcSw = 0, dcdcHw = 0;
};

// Akku-Cloud (bisher Dyness)
struct BatteryCloudData {
  bool valid = false;
  uint32_t atMs = 0;
  char sn[40] = "";
  char model[24] = "";
  float soc = NAN, soh = NAN, powerW = NAN, voltV = NAN, currA = NAN;
  float tempMax = NAN, tempMin = NAN, mosTemp = NAN;
  float cellVMax = NAN, cellVMin = NAN, cellDiffMv = NAN;
  float capKwh = NAN, remainKwh = NAN;
  float chargeLimA = NAN, dischargeLimA = NAN;
  float cycles = NAN;
  int8_t alarm = -1; // -1 unbekannt
  uint8_t cells = 0;
  float cellV[BAT_MAX_CELLS] = {};
};

struct BatteryInfo {
  BatteryHit hit;
  uint8_t packs = 0;
  uint32_t packsAtMs = 0;
  BatteryPack pack[BAT_MAX_PACKS];
  BatteryCloudData cloud;
  char cloudErr[64] = "";
  uint32_t cloudErrAtMs = 0;
  bool hitMissing = false; // HIT antwortet, liefert aber keine Werte im Block
};

namespace BatteryData {
void begin();
BatteryInfo snapshot();
void updateHit(const BatteryHit &h);       // h.valid=false: HIT liefert den Block nicht
void updatePacks(const BatteryPack *p, uint8_t n);
void updateCloud(const BatteryCloudData &c);
void setCloudError(const char *err);       // nullptr oder "" löscht
void toJson(JsonDocument &doc, const AppConfig &cfg);
}
