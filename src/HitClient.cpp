#include "HitClient.h"
#include "ModbusTcpRaw.h"
#include "DataSource.h"
#include <math.h>

namespace {
AppConfig local;
uint32_t nextPoll = 0;
bool started = false;

int16_t asI16(uint16_t u) {
  return (u >= 0x8000) ? int16_t(int32_t(u) - 0x10000) : int16_t(u);
}
int32_t asI32(uint16_t hi, uint16_t lo) {
  uint32_t v = (uint32_t(hi) << 16) | lo;
  if (v >= 0x80000000u) return int32_t(int64_t(v) - 0x100000000LL);
  return int32_t(v);
}
uint32_t asU32(uint16_t hi, uint16_t lo) {
  return (uint32_t(hi) << 16) | lo;
}

bool rd(uint16_t addr, uint16_t qty, uint16_t *out) {
  return ModbusTcpRaw::readInputRegs(local.hitHost, local.hitPort, local.hitUnitId, addr, qty, out);
}

bool doPollTcp() {
  uint16_t pv[7] = {}, volt[3] = {}, ipb[14] = {};
  uint16_t ePv[2] = {}, eImp[2] = {}, eExp[2] = {}, bat[2] = {}, soc[1] = {};

  bool ok = true;
  ok &= rd(29, 7, pv);     delay(60);
  ok &= rd(62, 3, volt);   delay(60);
  ok &= rd(1804, 14, ipb); delay(60);
  ok &= rd(2000, 2, ePv);  delay(60);
  ok &= rd(2040, 2, eImp); delay(60);
  ok &= rd(2048, 2, eExp); delay(60);
  ok &= rd(30009, 2, bat); delay(60);
  ok &= rd(30020, 1, soc);

  if (!ok) {
    gLive.hitOk = false;
    gLive.hitFails++;
    strlcpy(gLive.status, "HIT poll fail", sizeof(gLive.status));
    return false;
  }

  gLive.pvW = int32_t(pv[0]) + int32_t(pv[3]) + int32_t(pv[6]);
  gLive.v1 = volt[0] * 0.1f;
  gLive.v2 = volt[1] * 0.1f;
  gLive.v3 = volt[2] * 0.1f;
  gLive.i1 = asI16(ipb[0]) * 0.01f;
  gLive.i2 = asI16(ipb[1]) * 0.01f;
  gLive.i3 = asI16(ipb[2]) * 0.01f;
  gLive.gridW = -asI32(ipb[10], ipb[11]);
  gLive.energyPvKwh = asU32(ePv[0], ePv[1]) * 0.1;
  gLive.energyImpKwh = asU32(eImp[0], eImp[1]) * 0.1;
  gLive.energyExpKwh = asU32(eExp[0], eExp[1]) * 0.1;
  gLive.batW = asI32(bat[0], bat[1]);
  gLive.soc = soc[0];
  gLive.hitOk = true;
  gLive.lastHitMs = millis();
  gLive.dataOrigin = DataOrigin::Native;
  strlcpy(gLive.status, "HIT ok", sizeof(gLive.status));
  return true;
}
} // namespace

namespace HitClient {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  nextPoll = 0;
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
  nextPoll = 0;
}

bool doDemo() {
  const float t = millis() / 1000.0f;
  // Tagesähnliche PV-Kurve (langsam)
  float sun = fmaxf(0.0f, sinf((t / 60.0f) * 3.14159f));
  gLive.pvW = int32_t(800 + 4200 * sun);
  gLive.batW = int32_t(400 * sinf(t / 17.0f));
  gLive.gridW = int32_t(gLive.pvW + gLive.batW - 1200); // Hauslast ~1.2 kW
  gLive.soc = uint16_t(45 + 20 * sinf(t / 90.0f));
  if (gLive.soc > 100) gLive.soc = 100;
  gLive.v1 = 230.1f; gLive.v2 = 229.8f; gLive.v3 = 230.4f;
  gLive.i1 = gLive.pvW / 3.0f / 230.0f;
  gLive.i2 = gLive.i1; gLive.i3 = gLive.i1;
  gLive.energyPvKwh = 1000.0 + t / 3600.0; // Pseudodaten
  gLive.energyImpKwh = 100.0;
  gLive.energyExpKwh = 200.0;
  gLive.tasmotaOk = true;
  gLive.tasmotaPowerW = gLive.gridW;
  gLive.tasmotaTotalKwh = 111.1;
  gLive.tasmotaExportKwh = 222.2;
  strlcpy(gLive.tasmotaSensorKey, "DEMO", sizeof(gLive.tasmotaSensorKey));
  gLive.hitOk = true;
  gLive.lastHitMs = millis();
  gLive.lastTasmotaMs = millis();
  gLive.dataOrigin = DataOrigin::Demo;
  strlcpy(gLive.status, "DEMO — kein HIT", sizeof(gLive.status));
  return true;
}

bool pollNow() {
  if (local.demoMode) return doDemo();
  if (local.hitMode != HitLinkMode::Tcp) {
    strlcpy(gLive.status, "RS485 bald", sizeof(gLive.status));
    return false;
  }
  return doPollTcp();
}

void loop() {
  if (!started) return;
  if (millis() >= nextPoll) {
    nextPoll = millis() + max<uint32_t>(5000, local.hitPollMs);
    pollNow();
  }
}

} // namespace HitClient
