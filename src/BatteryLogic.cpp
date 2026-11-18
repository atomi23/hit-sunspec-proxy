#include "BatteryLogic.h"
#include <math.h>

namespace {
float u16x10(uint16_t r) { return (r == 0 || r == 0xFFFF) ? NAN : r / 10.0f; }
// Ströme dürfen 0 sein (BMS sperrt Laden bei vollem oder kaltem Akku)
float u16x10z(uint16_t r) { return r == 0xFFFF ? NAN : r / 10.0f; }
float i16(uint16_t r) { return r == 0x8000 ? NAN : float(int16_t(r)); }
} // namespace

bool decodeHitBattery(const uint16_t regs[12], BatteryHit &out) {
  bool any = false;
  for (int i = 0; i < 12; i++) any |= regs[i] != 0;
  out = BatteryHit{};
  out.atMs = millis();
  if (!any) return false;
  out.valid = true;
  out.maxChargeA = u16x10z(regs[0]);
  out.maxDischargeA = u16x10z(regs[1]);
  out.chargeCutV = u16x10(regs[2]);
  out.dischargeCutV = u16x10(regs[3]);
  out.modTempMax = i16(regs[4]);
  out.modTempMin = i16(regs[5]);
  out.cellTempMax = i16(regs[6]);
  out.cellTempMin = i16(regs[7]);
  out.modVMax = u16x10(regs[8]);
  out.modVMin = u16x10(regs[9]);
  out.cellVMax = u16x10(regs[10]);
  out.cellVMin = u16x10(regs[11]);
  return true;
}

String dynessStringToSign(const char *method, const char *contentMd5, const char *date, const char *path) {
  String s;
  s.reserve(96 + strlen(path));
  s += method;
  s += '\n';
  s += contentMd5;
  s += "\napplication/json\n";
  s += date;
  s += '\n';
  s += path;
  return s;
}

namespace {
float pointNum(JsonVariantConst v) {
  if (v.is<float>()) return v.as<float>();
  const char *s = v.as<const char *>();
  if (!s || !*s) return NAN;
  char *end = nullptr;
  const float f = strtof(s, &end);
  return end == s ? NAN : f;
}
} // namespace

// PowerBrick: Variante A (Punkt 600 vorhanden) oder B (10xxx-Serie); Punktnummern nach der HA-Integration
void dynessParsePoints(JsonArrayConst points, BatteryCloudData &io, bool keepCells) {
  // Punkte, die wir brauchen, in einer kleinen Tabelle sammeln
  struct P { uint32_t id; float v; };
  static const uint32_t WANT[] = {600, 700, 800, 900, 1200, 1300, 1500, 1800, 2000, 2300, 3800, 3900, 4100,
                                  12500, 12600, 12700, 12800, 13400, 13500, 13900, 18600, 19200};
  P got[sizeof(WANT) / sizeof(WANT[0])];
  for (size_t i = 0; i < sizeof(WANT) / sizeof(WANT[0]); i++) got[i] = {WANT[i], NAN};
  float cells[BAT_MAX_CELLS];
  for (float &c : cells) c = NAN;
  bool alarmSeen = false;
  float alarm = NAN;

  for (JsonVariantConst e : points) {
    const char *ids = e["pointId"].as<const char *>();
    const uint32_t id = ids ? uint32_t(strtoul(ids, nullptr, 10)) : e["pointId"].as<uint32_t>();
    if (!id) continue;
    if (id >= 10300 && id <= 11800 && id % 100 == 0) {
      const float v = pointNum(e["pointValue"]);
      if (!isnan(v) && v > 0) cells[(id - 10200) / 100 - 1] = v;
      continue;
    }
    for (P &p : got)
      if (p.id == id) {
        p.v = pointNum(e["pointValue"]);
        if (id == 4100) {
          alarmSeen = true;
          const char *s = e["pointValue"].as<const char *>();
          alarm = (s && *s) ? (strcmp(s, "0") != 0 ? 1.0f : 0.0f) : (isnan(p.v) ? NAN : (p.v != 0 ? 1.0f : 0.0f));
        }
      }
  }
  auto g = [&](uint32_t id) {
    for (const P &p : got)
      if (p.id == id) return p.v;
    return NAN;
  };

  if (!isnan(g(600))) { // Variante A
    io.voltV = g(600);
    io.currA = g(700);
    if (!isnan(g(800))) io.soc = g(800);
    io.soh = g(1200);
    io.cellVMax = g(1300);
    io.cellVMin = g(1500);
    io.tempMax = g(1800);
    io.tempMin = g(2000);
    io.mosTemp = g(2300);
    if (!isnan(g(900)) && g(900) > 0) io.cycles = g(900);
    if (!isnan(g(3800)) && g(3800) > 0) io.chargeLimA = g(3800);
    if (!isnan(g(3900)) && g(3900) > 0) io.dischargeLimA = g(3900);
    if (alarmSeen && !isnan(alarm)) io.alarm = int8_t(alarm);
  } else { // Variante B
    if (!isnan(g(13500))) io.voltV = g(13500);
    if (!isnan(g(13400))) io.currA = g(13400);
    if (!isnan(g(13900))) io.cycles = g(13900);
    float tmax = NAN, tmin = NAN;
    int nt = 0;
    for (uint32_t id : {12500u, 12600u, 12700u, 12800u}) {
      const float t = g(id);
      if (isnan(t) || t <= 0) continue;
      tmax = (nt == 0 || t > tmax) ? t : tmax;
      tmin = (nt == 0 || t < tmin) ? t : tmin;
      nt++;
    }
    if (nt) io.tempMax = tmax;
    if (nt > 1) io.tempMin = tmin;
    if (!isnan(g(18600)) && g(18600) > 0) io.chargeLimA = g(18600);
    if (!isnan(g(19200)) && g(19200) > 0) io.dischargeLimA = g(19200);
  }

  // Einzelzellen: Max/Min/Spreizung daraus, wenn nicht direkt geliefert
  float vmax = NAN, vmin = NAN;
  uint8_t n = 0;
  for (uint8_t i = 0; i < BAT_MAX_CELLS; i++) {
    if (isnan(cells[i])) continue;
    vmax = (n == 0 || cells[i] > vmax) ? cells[i] : vmax;
    vmin = (n == 0 || cells[i] < vmin) ? cells[i] : vmin;
    n++;
  }
  io.cells = 0;
  if (n) {
    if (isnan(io.cellVMax)) io.cellVMax = vmax;
    if (isnan(io.cellVMin)) io.cellVMin = vmin;
    io.cellDiffMv = (vmax - vmin) * 1000.0f;
    if (keepCells) {
      for (uint8_t i = 0; i < BAT_MAX_CELLS && !isnan(cells[i]); i++) io.cellV[io.cells++] = cells[i];
    }
  } else if (!isnan(io.cellVMax) && !isnan(io.cellVMin)) {
    io.cellDiffMv = (io.cellVMax - io.cellVMin) * 1000.0f;
  }
}

BatteryMerged mergeBattery(const BatteryInfo &b, uint32_t nowMs, uint32_t cloudIntervalMs) {
  BatteryMerged m;
  const bool cloud = b.cloud.valid && b.cloud.atMs && (nowMs - b.cloud.atMs) <= 2 * cloudIntervalMs;
  const bool hit = b.hit.valid;
  // Zellspannungen: Cloud ist mV-genau, der HIT nur auf 0,1 V; aus HIT-Werten keine Spreizung
  if (cloud && !isnan(b.cloud.cellVMax)) {
    m.cellVMax = b.cloud.cellVMax;
    m.cellVMin = b.cloud.cellVMin;
    m.cellDiffMv = b.cloud.cellDiffMv;
    m.srcCellV = "cloud";
  } else if (hit && !isnan(b.hit.cellVMax)) {
    m.cellVMax = b.hit.cellVMax;
    m.cellVMin = b.hit.cellVMin;
    m.srcCellV = "hit";
  }
  // Temperaturen: HIT liest öfter
  if (hit && !isnan(b.hit.cellTempMax)) {
    m.cellTempMax = b.hit.cellTempMax;
    m.cellTempMin = b.hit.cellTempMin;
    m.modTempMax = b.hit.modTempMax;
    m.modTempMin = b.hit.modTempMin;
    m.srcTemp = "hit";
  } else if (cloud && !isnan(b.cloud.tempMax)) {
    m.cellTempMax = b.cloud.tempMax;
    m.cellTempMin = b.cloud.tempMin;
    m.srcTemp = "cloud";
  }
  if (hit && !isnan(b.hit.maxChargeA)) {
    m.chargeLimA = b.hit.maxChargeA;
    m.dischargeLimA = b.hit.maxDischargeA;
    m.srcLim = "hit";
  } else if (cloud && !isnan(b.cloud.chargeLimA)) {
    m.chargeLimA = b.cloud.chargeLimA;
    m.dischargeLimA = b.cloud.dischargeLimA;
    m.srcLim = "cloud";
  }
  if (cloud) {
    m.cycles = b.cloud.cycles;
    m.remainKwh = b.cloud.remainKwh;
    m.alarm = b.cloud.alarm;
  }
  return m;
}

void decodeHitPack(const uint16_t regs[28], BatteryPack &out) {
  out = BatteryPack{};
  size_t n = 0;
  for (int i = 0; i < 16 && n < sizeof(out.sn) - 1; i++) {
    const char ch[2] = {char(regs[i] >> 8), char(regs[i] & 0xFF)};
    for (char c : ch) {
      if (!c || n >= sizeof(out.sn) - 1) goto done;
      out.sn[n++] = (c >= 0x20 && c < 0x7F) ? c : '?';
    }
  }
done:
  out.sn[n] = 0;
  out.dcdcSw = regs[20];
  out.dcdcHw = regs[21];
  out.bmsFw = (uint32_t(regs[22]) << 16) | regs[23];
  out.ratedCap = (uint32_t(regs[26]) << 16) | regs[27];
}
