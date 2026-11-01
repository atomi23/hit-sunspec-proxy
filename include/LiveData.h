#pragma once
#include <Arduino.h>
#include "DataSource.h"

struct LiveData {
  bool hitOk = false;
  bool ethOk = false;
  bool wifiOk = false;
  bool sunspecClient = false;
  bool mqttOk = false;
  bool mqttIngestOk = false;
  bool cloudOk = false;
  DataOrigin dataOrigin = DataOrigin::None;
  uint32_t lastHitMs = 0;
  uint32_t lastMqttIngestMs = 0;
  uint32_t lastCloudMs = 0;
  uint32_t lastMapMs = 0;
  uint32_t hitFails = 0;

  int32_t pvW = 0;
  int32_t batW = 0;
  int32_t gridW = 0;
  uint16_t soc = 0;

  float v1 = 230, v2 = 230, v3 = 230;
  float i1 = 0, i2 = 0, i3 = 0;

  double energyPvKwh = 0;
  double energyExpKwh = 0; // HIT Grid Sell
  double energyImpKwh = 0; // HIT Grid Buy

  // Tasmota Zähler (optional)
  bool tasmotaOk = false;
  int32_t tasmotaPowerW = 0;   // +Bezug typisch
  double tasmotaTotalKwh = 0;  // Bezug E_in
  double tasmotaExportKwh = 0; // Einspeisung E_out
  char tasmotaMeterId[24] = "";
  char tasmotaSensorKey[24] = ""; // z.B. "E320"
  uint32_t lastTasmotaMs = 0;

  // SunSpec Model 123 (Consolinno LPP) — SF-2: 10000 = 100.00%
  uint16_t wMaxLimPct = 10000;
  bool wMaxLimEna = false;

  // Letzte HIT-Schreibwerte
  uint16_t ctrlChargePct = 100;
  uint16_t ctrlDischargePct = 100;
  uint16_t ctrlApcPct10 = 1000;
  bool ctrlApcEna = false;

  char status[48] = "boot";
  char ipEth[16] = "-";
  char ipWifi[16] = "-";
  char ipShown[16] = "-";
  bool setupScreen = false;
};

inline LiveData gLive;
