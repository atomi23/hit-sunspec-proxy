#pragma once
#include <Arduino.h>

enum KnxChId : uint8_t {
  KNX_PV_W = 0,
  KNX_PV1_W, KNX_PV1_V, KNX_PV1_A,
  KNX_PV2_W, KNX_PV2_V, KNX_PV2_A,
  KNX_PV3_W, KNX_PV3_V, KNX_PV3_A,
  KNX_PV4_W, KNX_PV4_V, KNX_PV4_A,
  KNX_BAT_W, KNX_BAT_V, KNX_BAT_A, KNX_SOC, KNX_SOH,
  KNX_GRID_W, KNX_GRID_W1, KNX_GRID_W2, KNX_GRID_W3,
  KNX_V1, KNX_V2, KNX_V3, KNX_I1, KNX_I2, KNX_I3, KNX_FREQ,
  KNX_EPV, KNX_EIMP, KNX_EEXP,
  KNX_TAS_W, KNX_TAS_IN, KNX_TAS_OUT,
  KNX_LPP,
  KNX_CH_COUNT
};

struct KnxCh {
  bool ena = false;
  char ga[14] = "";
  uint16_t dpt = 0; // 0 = Default aus Katalog
};

enum class KnxValKind : uint8_t { PowerW, Percent, Volt, Amp, Hz, EnergyKwh, RatioPct };

struct KnxPointMeta {
  const char *id;
  const char *label;
  uint16_t defaultDpt;
  KnxValKind kind;
};

inline const KnxPointMeta &knxMeta(uint8_t i) {
  static const KnxPointMeta k[KNX_CH_COUNT] = {
      {"pv_w", "PV Leistung gesamt", 14056, KnxValKind::PowerW},
      {"pv1_w", "MPPT1 Leistung", 14056, KnxValKind::PowerW},
      {"pv1_v", "MPPT1 Spannung", 14027, KnxValKind::Volt},
      {"pv1_a", "MPPT1 Strom", 14033, KnxValKind::Amp},
      {"pv2_w", "MPPT2 Leistung", 14056, KnxValKind::PowerW},
      {"pv2_v", "MPPT2 Spannung", 14027, KnxValKind::Volt},
      {"pv2_a", "MPPT2 Strom", 14033, KnxValKind::Amp},
      {"pv3_w", "MPPT3 Leistung", 14056, KnxValKind::PowerW},
      {"pv3_v", "MPPT3 Spannung", 14027, KnxValKind::Volt},
      {"pv3_a", "MPPT3 Strom", 14033, KnxValKind::Amp},
      {"pv4_w", "MPPT4 Leistung", 14056, KnxValKind::PowerW},
      {"pv4_v", "MPPT4 Spannung", 14027, KnxValKind::Volt},
      {"pv4_a", "MPPT4 Strom", 14033, KnxValKind::Amp},
      {"bat_w", "Batterie Leistung", 14056, KnxValKind::PowerW},
      {"bat_v", "Batterie Spannung", 14027, KnxValKind::Volt},
      {"bat_a", "Batterie Strom", 14033, KnxValKind::Amp},
      {"soc", "Batterie SoC", 5001, KnxValKind::Percent},
      {"soh", "Batterie SoH", 5001, KnxValKind::Percent},
      {"grid_w", "Netz Leistung gesamt", 14056, KnxValKind::PowerW},
      {"grid_w1", "Netz Leistung L1", 14056, KnxValKind::PowerW},
      {"grid_w2", "Netz Leistung L2", 14056, KnxValKind::PowerW},
      {"grid_w3", "Netz Leistung L3", 14056, KnxValKind::PowerW},
      {"v1", "AC Spannung L1", 14027, KnxValKind::Volt},
      {"v2", "AC Spannung L2", 14027, KnxValKind::Volt},
      {"v3", "AC Spannung L3", 14027, KnxValKind::Volt},
      {"i1", "AC Strom L1", 14033, KnxValKind::Amp},
      {"i2", "AC Strom L2", 14033, KnxValKind::Amp},
      {"i3", "AC Strom L3", 14033, KnxValKind::Amp},
      {"freq", "Netzfrequenz", 14035, KnxValKind::Hz},
      {"epv", "PV Energie", 13010, KnxValKind::EnergyKwh},
      {"eimp", "Bezug Energie HIT", 13010, KnxValKind::EnergyKwh},
      {"eexp", "Einspeisung Energie HIT", 13010, KnxValKind::EnergyKwh},
      {"tas_w", "Zähler Leistung Tasmota", 14056, KnxValKind::PowerW},
      {"tas_in", "Zähler E_in Tasmota", 13010, KnxValKind::EnergyKwh},
      {"tas_out", "Zähler E_out Tasmota", 13010, KnxValKind::EnergyKwh},
      {"lpp", "Consolinno LPP %", 5001, KnxValKind::RatioPct},
  };
  return k[i < KNX_CH_COUNT ? i : 0];
}
