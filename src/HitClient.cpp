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
  // MPPT: typisch W / V*0.1 / I*0.01 je String ab Reg 29 (4×3)
  uint16_t mppt[12] = {};
  uint16_t volt[3] = {}, ipb[14] = {}, freq[1] = {};
  uint16_t ePv[2] = {}, eImp[2] = {}, eExp[2] = {}, bat[2] = {};
  uint16_t batInfo[12] = {};

  bool ok = true;
  ok &= rd(29, 12, mppt);  delay(60);
  ok &= rd(62, 3, volt);    delay(40);
  ok &= rd(70, 1, freq);    delay(40);
  ok &= rd(1804, 14, ipb);  delay(60);
  ok &= rd(2000, 2, ePv);   delay(40);
  ok &= rd(2040, 2, eImp);  delay(40);
  ok &= rd(2048, 2, eExp);  delay(40);
  ok &= rd(30009, 2, bat);  delay(40);
  bool batOk = rd(30014, 12, batInfo);
  delay(40);

  if (!ok) {
    gLive.hitOk = false;
    gLive.hitFails++;
    strlcpy(gLive.status, "HIT poll fail", sizeof(gLive.status));
    return false;
  }

  auto takeMppt = [&](int base, int32_t &w, float &v, float &a) {
    w = int32_t(mppt[base]);
    v = mppt[base + 1] * 0.1f;
    a = asI16(mppt[base + 2]) * 0.01f;
  };
  takeMppt(0, gLive.pv1W, gLive.pv1V, gLive.pv1A);
  takeMppt(3, gLive.pv2W, gLive.pv2V, gLive.pv2A);
  takeMppt(6, gLive.pv3W, gLive.pv3V, gLive.pv3A);
  takeMppt(9, gLive.pv4W, gLive.pv4V, gLive.pv4A);
  gLive.pvW = gLive.pv1W + gLive.pv2W + gLive.pv3W + gLive.pv4W;

  gLive.v1 = volt[0] * 0.1f;
  gLive.v2 = volt[1] * 0.1f;
  gLive.v3 = volt[2] * 0.1f;
  gLive.freqHz = freq[0] > 4000 ? freq[0] * 0.01f : (freq[0] > 0 ? float(freq[0]) : 50.0f);
  gLive.i1 = asI16(ipb[0]) * 0.01f;
  gLive.i2 = asI16(ipb[1]) * 0.01f;
  gLive.i3 = asI16(ipb[2]) * 0.01f;
  // Phasenleistungen oft in Block; Fallback anteilig
  gLive.gridW1 = -asI32(ipb[4], ipb[5]);
  gLive.gridW2 = -asI32(ipb[6], ipb[7]);
  gLive.gridW3 = -asI32(ipb[8], ipb[9]);
  gLive.gridW = -asI32(ipb[10], ipb[11]);
  if (gLive.gridW1 == 0 && gLive.gridW2 == 0 && gLive.gridW3 == 0 && gLive.gridW != 0) {
    gLive.gridW1 = gLive.gridW / 3;
    gLive.gridW2 = gLive.gridW / 3;
    gLive.gridW3 = gLive.gridW - gLive.gridW1 - gLive.gridW2;
  }

  gLive.energyPvKwh = asU32(ePv[0], ePv[1]) * 0.1;
  gLive.energyImpKwh = asU32(eImp[0], eImp[1]) * 0.1;
  gLive.energyExpKwh = asU32(eExp[0], eExp[1]) * 0.1;
  gLive.batW = asI32(bat[0], bat[1]);

  if (batOk) {
    gLive.batV = batInfo[0] * 0.1f;
    // Strom ggf. in Block; sonst aus P/U
    float rawA = asI16(batInfo[1]) * 0.01f;
    gLive.batA = (fabsf(rawA) > 0.001f) ? rawA
                                        : (gLive.batV > 1.0f ? float(gLive.batW) / gLive.batV : 0);
    gLive.soc = batInfo[6];
    gLive.soh = batInfo[7];
    if (gLive.soc > 100 && gLive.soc <= 1000) gLive.soc /= 10;
    if (gLive.soh > 100 && gLive.soh <= 1000) gLive.soh /= 10;
    if (gLive.soc > 100) gLive.soc = 100;
    if (gLive.soh > 100) gLive.soh = 100;
  } else {
    gLive.batA = gLive.batV > 1.0f ? float(gLive.batW) / gLive.batV : 0;
  }

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
  float sun = fmaxf(0.0f, sinf((t / 60.0f) * 3.14159f));
  gLive.pvW = int32_t(800 + 4200 * sun);
  gLive.pv1W = gLive.pvW / 4;
  gLive.pv2W = gLive.pvW / 4;
  gLive.pv3W = gLive.pvW / 4;
  gLive.pv4W = gLive.pvW - gLive.pv1W - gLive.pv2W - gLive.pv3W;
  gLive.pv1V = 380;
  gLive.pv2V = 375;
  gLive.pv3V = 390;
  gLive.pv4V = 370;
  gLive.pv1A = gLive.pv1V > 1 ? gLive.pv1W / gLive.pv1V : 0;
  gLive.pv2A = gLive.pv2V > 1 ? gLive.pv2W / gLive.pv2V : 0;
  gLive.pv3A = gLive.pv3V > 1 ? gLive.pv3W / gLive.pv3V : 0;
  gLive.pv4A = gLive.pv4V > 1 ? gLive.pv4W / gLive.pv4V : 0;
  gLive.batW = int32_t(400 * sinf(t / 17.0f));
  gLive.batV = 51.2f;
  gLive.batA = gLive.batW / gLive.batV;
  gLive.gridW = int32_t(gLive.pvW + gLive.batW - 1200);
  gLive.gridW1 = gLive.gridW / 3;
  gLive.gridW2 = gLive.gridW / 3;
  gLive.gridW3 = gLive.gridW - gLive.gridW1 - gLive.gridW2;
  gLive.soc = uint16_t(45 + 20 * sinf(t / 90.0f));
  if (gLive.soc > 100) gLive.soc = 100;
  gLive.soh = 98;
  gLive.v1 = 230.1f;
  gLive.v2 = 229.8f;
  gLive.v3 = 230.4f;
  gLive.freqHz = 50.02f;
  gLive.i1 = gLive.pvW / 3.0f / 230.0f;
  gLive.i2 = gLive.i1;
  gLive.i3 = gLive.i1;
  gLive.energyPvKwh = 1000.0 + t / 3600.0;
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
