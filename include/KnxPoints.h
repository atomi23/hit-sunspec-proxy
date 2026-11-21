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
  // Akkudetails; nur hinten anhängen, die gespeicherte Zuordnung hängt an der Position
  KNX_AKKU_TMAX, KNX_AKKU_TMIN, KNX_AKKU_VMAX, KNX_AKKU_VMIN, KNX_AKKU_VDIFF,
  KNX_AKKU_CYCLES, KNX_AKKU_REMAIN, KNX_AKKU_ALARM,
  KNX_DISPLAY_SWITCH,
  KNX_CH_COUNT
};

struct KnxCh {
  bool ena = false;
  char ga[14] = "";
  uint16_t dpt = 0; // 0 = Default aus Katalog
};

enum class KnxValKind : uint8_t { PowerW, Percent, Volt, Amp, Hz, EnergyKwh, RatioPct, Temp, MilliVolt, Count, Flag, Switch };

struct KnxPointMeta {
  const char *id;
  const char *label;
  const char *labelEn;
  uint16_t defaultDpt;
  KnxValKind kind;
};

inline const KnxPointMeta &knxMeta(uint8_t i) {
  static const KnxPointMeta k[KNX_CH_COUNT] = {
      {"pv_w", "PV Leistung gesamt", "Total PV power", 14056, KnxValKind::PowerW},
      {"pv1_w", "MPPT1 Leistung", "MPPT1 power", 14056, KnxValKind::PowerW},
      {"pv1_v", "MPPT1 Spannung", "MPPT1 voltage", 14027, KnxValKind::Volt},
      {"pv1_a", "MPPT1 Strom", "MPPT1 current", 14019, KnxValKind::Amp},
      {"pv2_w", "MPPT2 Leistung", "MPPT2 power", 14056, KnxValKind::PowerW},
      {"pv2_v", "MPPT2 Spannung", "MPPT2 voltage", 14027, KnxValKind::Volt},
      {"pv2_a", "MPPT2 Strom", "MPPT2 current", 14019, KnxValKind::Amp},
      {"pv3_w", "MPPT3 Leistung", "MPPT3 power", 14056, KnxValKind::PowerW},
      {"pv3_v", "MPPT3 Spannung", "MPPT3 voltage", 14027, KnxValKind::Volt},
      {"pv3_a", "MPPT3 Strom", "MPPT3 current", 14019, KnxValKind::Amp},
      {"pv4_w", "MPPT4 Leistung", "MPPT4 power", 14056, KnxValKind::PowerW},
      {"pv4_v", "MPPT4 Spannung", "MPPT4 voltage", 14027, KnxValKind::Volt},
      {"pv4_a", "MPPT4 Strom", "MPPT4 current", 14019, KnxValKind::Amp},
      {"bat_w", "Batterie Leistung", "Battery power", 14056, KnxValKind::PowerW},
      {"bat_v", "Batterie Spannung", "Battery voltage", 14027, KnxValKind::Volt},
      {"bat_a", "Batterie Strom", "Battery current", 14019, KnxValKind::Amp},
      {"soc", "Batterie SoC", "Battery SoC", 5001, KnxValKind::Percent},
      {"soh", "Batterie SoH", "Battery SoH", 5001, KnxValKind::Percent},
      {"grid_w", "Netz Leistung gesamt", "Total grid power", 14056, KnxValKind::PowerW},
      {"grid_w1", "Netz Leistung L1", "Grid power L1", 14056, KnxValKind::PowerW},
      {"grid_w2", "Netz Leistung L2", "Grid power L2", 14056, KnxValKind::PowerW},
      {"grid_w3", "Netz Leistung L3", "Grid power L3", 14056, KnxValKind::PowerW},
      {"v1", "AC Spannung L1", "AC voltage L1", 14027, KnxValKind::Volt},
      {"v2", "AC Spannung L2", "AC voltage L2", 14027, KnxValKind::Volt},
      {"v3", "AC Spannung L3", "AC voltage L3", 14027, KnxValKind::Volt},
      {"i1", "AC Strom L1", "AC current L1", 14019, KnxValKind::Amp},
      {"i2", "AC Strom L2", "AC current L2", 14019, KnxValKind::Amp},
      {"i3", "AC Strom L3", "AC current L3", 14019, KnxValKind::Amp},
      {"freq", "Netzfrequenz", "Grid frequency", 14033, KnxValKind::Hz},
      {"epv", "PV Energie", "PV energy", 13010, KnxValKind::EnergyKwh},
      {"eimp", "Bezug Energie HIT", "HIT imported energy", 13010, KnxValKind::EnergyKwh},
      {"eexp", "Einspeisung Energie HIT", "HIT exported energy", 13010, KnxValKind::EnergyKwh},
      {"tas_w", "Zähler Leistung Tasmota", "Tasmota meter power", 14056, KnxValKind::PowerW},
      {"tas_in", "Zähler E_in Tasmota", "Tasmota meter E_in", 13010, KnxValKind::EnergyKwh},
      {"tas_out", "Zähler E_out Tasmota", "Tasmota meter E_out", 13010, KnxValKind::EnergyKwh},
      {"lpp", "Consolinno LPP %", "Consolinno LPP %", 5001, KnxValKind::RatioPct},
      {"akku_tmax", "Zelltemperatur max", "Maximum cell temperature", 9001, KnxValKind::Temp},
      {"akku_tmin", "Zelltemperatur min", "Minimum cell temperature", 9001, KnxValKind::Temp},
      {"akku_vmax", "Zellspannung max", "Maximum cell voltage", 9020, KnxValKind::MilliVolt},
      {"akku_vmin", "Zellspannung min", "Minimum cell voltage", 9020, KnxValKind::MilliVolt},
      {"akku_vdiff", "Zellspannung Spreizung", "Cell voltage spread", 9020, KnxValKind::MilliVolt},
      {"akku_cycles", "Ladezyklen", "Charge cycles", 12001, KnxValKind::Count},
      {"akku_remain", "Restenergie", "Remaining energy", 13013, KnxValKind::EnergyKwh},
      {"akku_alarm", "Akku-Alarm", "Battery alarm", 1005, KnxValKind::Flag},
      {"display_switch", "Display schalten (Empfang)", "Display switch (receive)", 1001, KnxValKind::Switch},
  };
  return k[i < KNX_CH_COUNT ? i : 0];
}
