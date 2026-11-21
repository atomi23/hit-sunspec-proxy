#include "HitClient.h"
#include "HitBus.h"
#include "DataSource.h"
#include "EventLog.h"
#include "BatteryData.h"
#include "BatteryLogic.h"
#include <WiFi.h>
#if defined(ETH_PHY_TYPE) || defined(BOARD_T_CONNECT_PRO) || defined(BOARD_T_POE_PRO) || defined(BOARD_WT32_ETH01) || defined(BOARD_P4_POE_ETH)
#include <ETH.h>
#endif
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace {
AppConfig local;
uint32_t nextPoll = 0;
bool started = false;
uint32_t failStreak = 0;
uint32_t failSince = 0;
SemaphoreHandle_t cfgLock = nullptr;
AppConfig pendingCfg;
bool cfgPending = false;
volatile bool forcePoll = false;
uint32_t batNext = 0, packNext = 0;
bool batFailLogged = false;

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
  return HitBus::readInput(addr, qty, out);
}

void pollBattery();

bool doPoll() {
  // PV1..4 ab Reg 27, je String V×0.1 / I×0.01 / W (V2.8)
  uint16_t mppt[12] = {};
  uint16_t volt[3] = {}, ipb[14] = {}, freq[1] = {};
  uint16_t ePv[2] = {}, eImp[2] = {}, eExp[2] = {}, bat[2] = {};
  // V2.8: 1909 Soc, 1910 Soh, 1911 Voltage×0.1, 1912.. Current I32×0.1; 30020 Soc (Backup)
  uint16_t batInfo[6] = {}, batSocAlt[1] = {};

  struct Blk { uint16_t addr, qty; uint16_t *dst; uint16_t pause; };
  const Blk blocks[] = {{27, 12, mppt, 60},   {62, 3, volt, 40},    {66, 1, freq, 40},
                        {1804, 14, ipb, 60},  {2000, 2, ePv, 40},   {2040, 2, eImp, 40},
                        {2048, 2, eExp, 40},  {30009, 2, bat, 40},  {1909, 6, batInfo, 40}};
  gLive.hitPolls++;
  uint16_t failAddr = 0;
  for (const Blk &b : blocks) {
    if (!rd(b.addr, b.qty, b.dst)) {
      failAddr = b.addr;
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(b.pause));
  }

  if (failAddr) {
    gLive.hitOk = false;
    gLive.hitFails++;
    failStreak++;
    if (failStreak == 1) {
      failSince = millis();
      EventLog::add('W', "HIT", "Abfrage fehlgeschlagen bei Reg %u: %s", failAddr, HitBus::lastError());
    } else if (failStreak % 20 == 0) {
      EventLog::add('E', "HIT", "seit %lu s keine Daten (%lu Versuche): %s",
                    (unsigned long)((millis() - failSince) / 1000), (unsigned long)failStreak,
                    HitBus::lastError());
    }
    strlcpy(gLive.status, "HIT poll fail", sizeof(gLive.status));
    return false;
  }
  if (failStreak) {
    EventLog::add('I', "HIT", "wieder erreichbar nach %lu Fehlversuch%s (%lu s)", (unsigned long)failStreak,
                  failStreak == 1 ? "" : "en", (unsigned long)((millis() - failSince) / 1000));
    failStreak = 0;
  }
  const bool socAltOk = rd(30020, 1, batSocAlt);
  vTaskDelay(pdMS_TO_TICKS(40));

  auto takeMppt = [&](int base, int32_t &w, float &v, float &a) {
    v = mppt[base] * 0.1f;
    a = asI16(mppt[base + 1]) * 0.01f;
    w = int32_t(mppt[base + 2]);
  };
  takeMppt(0, gLive.pv1W, gLive.pv1V, gLive.pv1A);
  takeMppt(3, gLive.pv2W, gLive.pv2V, gLive.pv2A);
  takeMppt(6, gLive.pv3W, gLive.pv3V, gLive.pv3A);
  takeMppt(9, gLive.pv4W, gLive.pv4V, gLive.pv4A);
  gLive.pvW = gLive.pv1W + gLive.pv2W + gLive.pv3W + gLive.pv4W;

  gLive.v1 = volt[0] * 0.1f;
  gLive.v2 = volt[1] * 0.1f;
  gLive.v3 = volt[2] * 0.1f;
  // 1807 Zähler / 66 Wechselrichter, beide Hz×100
  auto plausibleHz = [](uint16_t raw) { return raw >= 4500 && raw <= 6500; };
  gLive.freqHz = plausibleHz(ipb[3]) ? ipb[3] * 0.01f : (plausibleHz(freq[0]) ? freq[0] * 0.01f : 50.0f);
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

  gLive.soc = batInfo[0];
  gLive.soh = batInfo[1];
  gLive.batV = batInfo[2] * 0.1f;
  // Strom: I32 ab 1912 (×0.1), sonst aus P/U
  const float rawA = asI32(batInfo[3], batInfo[4]) * 0.1f;
  gLive.batA = (fabsf(rawA) > 0.001f) ? rawA
                                      : (gLive.batV > 1.0f ? float(gLive.batW) / gLive.batV : 0);
  if (socAltOk && batSocAlt[0] && batSocAlt[0] <= 1000) {
    uint16_t s = batSocAlt[0];
    if (s > 100) s /= 10;
    if (s <= 100) gLive.soc = s;
  }
  if (gLive.soc > 100 && gLive.soc <= 1000) gLive.soc /= 10;
  if (gLive.soh > 100 && gLive.soh <= 1000) gLive.soh /= 10;
  if (gLive.soc > 100) gLive.soc = 100;
  if (gLive.soh > 100) gLive.soh = 100;

  gLive.hitOk = true;
  gLive.lastHitMs = millis();
  gLive.dataOrigin = DataOrigin::Native;
  strlcpy(gLive.status, "HIT ok", sizeof(gLive.status));
  pollBattery();
  return true;
}

void takeConfig() {
  if (!cfgLock) return;
  xSemaphoreTake(cfgLock, portMAX_DELAY);
  if (cfgPending) {
    local = pendingCfg;
    cfgPending = false;
    nextPoll = 0;
  }
  xSemaphoreGive(cfgLock);
}

bool doDemo();
bool pollOnce();

// Zusatzwerte des Akkus in eigenem Takt, nur nach erfolgreichem Haupt-Poll: ein Lesebefehl über 12 Register,
// die Packinfo stündlich. Fehler hier ändern nichts an hitOk.
void pollBattery() {
  if (!local.batEnabled || !local.batHitEnabled) return;
  const uint32_t now = millis();
  if (batNext && int32_t(now - batNext) < 0) return;
  batNext = (now + uint32_t(constrain(local.batHitPollS, 30, 3600)) * 1000UL) | 1;
  uint16_t regs[12] = {};
  BatteryHit h;
  if (!rd(1916, 12, regs)) {
    if (!batFailLogged) EventLog::add('W', "Akku", "HIT liefert keine Akkudetails: %s", HitBus::lastError());
    batFailLogged = true;
    BatteryData::updateHit(h);
    return;
  }
  batFailLogged = false;
  decodeHitBattery(regs, h);
  BatteryData::updateHit(h);
  vTaskDelay(pdMS_TO_TICKS(40));
  if (packNext && int32_t(now - packNext) < 0) return;
  packNext = (now + 3600000UL) | 1;
  uint16_t sys[5] = {};
  if (!HitBus::readHolding(8000, 5, sys)) return;
  const uint8_t n = uint8_t(min<uint16_t>(sys[2], BAT_MAX_PACKS));
  BatteryPack packs[BAT_MAX_PACKS];
  uint8_t got = 0;
  for (uint8_t i = 0; i < n; i++) {
    vTaskDelay(pdMS_TO_TICKS(40));
    uint16_t pr[28] = {};
    if (!HitBus::readHolding(uint16_t(8100 + 50 * i), 28, pr)) break;
    decodeHitPack(pr, packs[got++]);
  }
  BatteryData::updatePacks(packs, got);
}

bool networkReady() {
  if (local.demoMode || local.hitMode == HitLinkMode::RtuRs485) return true;
#if defined(ETH_PHY_TYPE) || defined(BOARD_T_CONNECT_PRO) || defined(BOARD_T_POE_PRO) || defined(BOARD_WT32_ETH01) || defined(BOARD_P4_POE_ETH)
  if (local.useEthernet && ETH.linkUp() && uint32_t(ETH.localIP()) != 0) return true;
#endif
  const wifi_mode_t mode = WiFi.getMode();
  return (mode == WIFI_STA || mode == WIFI_AP_STA) && WiFi.status() == WL_CONNECTED &&
         uint32_t(WiFi.localIP()) != 0;
}

// HIT-Poll dauert mit Timeouts leicht mehrere Sekunden — eigene Task, sonst
// verhungern SunSpec (Consolinno) und die Weboberfläche.
void taskMain(void *) {
  for (;;) {
    takeConfig();
    if (started && !networkReady()) {
      nextPoll = millis();
    } else if (started && (forcePoll || int32_t(millis() - nextPoll) >= 0)) {
      forcePoll = false;
      nextPoll = millis() + max<uint32_t>(5000, local.hitPollMs);
      pollOnce();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
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
  if (local.batEnabled) {
    BatteryHit h;
    h.valid = true;
    h.atMs = millis();
    h.maxChargeA = 50; h.maxDischargeA = 50; h.chargeCutV = 56.8f; h.dischargeCutV = 44.8f;
    h.cellTempMax = 21 + 3 * sinf(t / 300.0f); h.cellTempMin = h.cellTempMax - 3;
    h.modTempMax = h.cellTempMax + 1; h.modTempMin = h.cellTempMin;
    h.cellVMax = 3.3f; h.cellVMin = 3.3f; h.modVMax = 52.9f; h.modVMin = 52.7f;
    BatteryData::updateHit(h);
    BatteryPack p;
    strlcpy(p.sn, "DEMO-PACK-0001", sizeof(p.sn));
    p.bmsFw = 10203; p.ratedCap = 5120;
    BatteryData::updatePacks(&p, 1);
  }
  strlcpy(gLive.status, "DEMO — kein HIT", sizeof(gLive.status));
  return true;
}

bool pollOnce() {
  const bool ok = local.demoMode ? doDemo() : doPoll();
  gLive.hitPollsDone++;
  return ok;
}

} // namespace

namespace HitClient {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  nextPoll = 0;
  cfgLock = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(taskMain, "hit", 8192, nullptr, 1, nullptr, 0);
}

void applyConfig(const AppConfig &cfg) {
  if (!cfgLock) {
    local = cfg;
    nextPoll = 0;
    return;
  }
  xSemaphoreTake(cfgLock, portMAX_DELAY);
  pendingCfg = cfg;
  cfgPending = true;
  xSemaphoreGive(cfgLock);
}

bool pollNow() {
  // Nur anstoßen: die HIT-Task pollt selbst. Ein zusätzlicher synchroner Poll lief sonst
  // parallel zur Task (geteilte Zähler, gLive von zwei Tasks beschrieben) und blockierte setup().
  forcePoll = true;
  return true;
}

void loop() {}

} // namespace HitClient
