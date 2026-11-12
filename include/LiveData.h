#pragma once
#include <Arduino.h>
#include "DataSource.h"

struct LiveData {
  bool hitOk = false;
  bool ethOk = false;
  bool wifiOk = false;
  bool sunspecClient = false;
  uint8_t gatewayClients = 0;
  bool mqttOk = false;
  bool mqttIngestOk = false;
  bool cloudOk = false;
  bool knxOk = false;
  DataOrigin dataOrigin = DataOrigin::None;
  uint32_t lastHitMs = 0;
  uint32_t lastMqttIngestMs = 0;
  uint32_t lastCloudMs = 0;
  uint32_t lastKnxMs = 0;
  uint32_t lastMapMs = 0;
  uint32_t hitFails = 0;
  uint32_t hitPolls = 0;

  int32_t pvW = 0;
  int32_t pv1W = 0, pv2W = 0, pv3W = 0, pv4W = 0;
  float pv1V = 0, pv2V = 0, pv3V = 0, pv4V = 0;
  float pv1A = 0, pv2A = 0, pv3A = 0, pv4A = 0;

  int32_t batW = 0;
  float batV = 0;
  float batA = 0;
  uint16_t soc = 0;
  uint16_t soh = 0;

  int32_t gridW = 0;
  int32_t gridW1 = 0, gridW2 = 0, gridW3 = 0;
  float v1 = 230, v2 = 230, v3 = 230;
  float i1 = 0, i2 = 0, i3 = 0;
  float freqHz = 50;

  double energyPvKwh = 0;
  double energyExpKwh = 0;
  double energyImpKwh = 0;

  bool tasmotaOk = false;
  int32_t tasmotaPowerW = 0;
  double tasmotaTotalKwh = 0;
  double tasmotaExportKwh = 0;
  char tasmotaMeterId[24] = "";
  char tasmotaSensorKey[24] = "";
  uint32_t lastTasmotaMs = 0;

  uint16_t wMaxLimPct = 10000;
  bool wMaxLimEna = false;

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
