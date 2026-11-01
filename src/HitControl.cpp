#include "HitControl.h"
#include "AppConfig.h"
#include "ModbusTcpRaw.h"
#include "LiveData.h"

namespace {
AppConfig local;
String err;
bool started = false;

bool wr(uint16_t addr, uint16_t val) {
  if (local.hitMode != HitLinkMode::Tcp) {
    err = "RS485 Write noch nicht";
    return false;
  }
  bool ok = ModbusTcpRaw::writeHoldingReg(local.hitHost, local.hitPort, local.hitUnitId, addr, val);
  if (!ok) err = String("Write Reg ") + addr + " fail";
  delay(80);
  return ok;
}

uint16_t clampU(uint16_t v, uint16_t lo, uint16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
} // namespace

namespace HitControl {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  err = "";
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
}

String lastError() { return err; }

bool applyBatteryLimits() {
  if (local.demoMode) {
    err = "DEMO: Write nur simuliert (kein HIT)";
    gLive.ctrlChargePct = local.batMaxChargePct;
    gLive.ctrlDischargePct = local.batMaxDischargePct;
    strlcpy(gLive.status, "DEMO write ok", sizeof(gLive.status));
    return true;
  }
  if (!local.hitWriteEnabled) {
    err = "Steuerung aus (Web: hitWriteEnabled)";
    return false;
  }
  err = "";
  uint16_t c = clampU(local.batMaxChargePct, 10, 100);
  uint16_t d = clampU(local.batMaxDischargePct, 10, 100);
  uint16_t mx = clampU(local.batMaxSoc, 10, 100);
  uint16_t mn = clampU(local.batMinSoc, 10, 90);
  bool ok = true;
  ok &= wr(306, c); // Battery Max Charge Power
  ok &= wr(307, d); // Battery Max Discharge Power
  ok &= wr(308, mx);
  ok &= wr(309, mn);
  if (ok) {
    gLive.ctrlChargePct = c;
    gLive.ctrlDischargePct = d;
    strlcpy(gLive.status, "HIT write ok", sizeof(gLive.status));
  }
  return ok;
}

bool applyApcLimit() {
  if (local.demoMode) {
    err = "DEMO: APC nur simuliert";
    gLive.ctrlApcEna = local.apcEnabled;
    gLive.ctrlApcPct10 = local.apcPwrLmtPct10;
    return true;
  }
  if (!local.hitWriteEnabled) {
    err = "Steuerung aus";
    return false;
  }
  err = "";
  // APC im Safety-Bereich 101–299 — bewusst nur wenn aktiviert
  uint16_t lim = clampU(local.apcPwrLmtPct10, 0, 1000);
  bool ok = wr(279, local.apcEnabled ? 1 : 0);
  ok &= wr(280, lim);
  if (ok) {
    gLive.ctrlApcEna = local.apcEnabled;
    gLive.ctrlApcPct10 = lim;
  }
  return ok;
}

bool applyAll() {
  bool ok = applyBatteryLimits();
  if (local.apcEnabled || local.apcPwrLmtPct10 < 1000) {
    ok &= applyApcLimit();
  }
  return ok;
}

bool onSunSpecPowerLimit(uint16_t pctSF2, bool enabled) {
  gLive.wMaxLimPct = pctSF2;
  gLive.wMaxLimEna = enabled;

  if (local.demoMode) {
    // Debug-Seite sieht den Write; HIT bleibt unberührt
    return true;
  }
  if (!local.hitWriteEnabled || !local.sunspecWriteToHit) return true;

  // Map SunSpec % (SF-2) → HIT APC %*10
  uint16_t pct10 = uint16_t((uint32_t(pctSF2) + 5) / 10); // 10000 → 1000
  if (pct10 > 1000) pct10 = 1000;
  local.apcEnabled = enabled;
  local.apcPwrLmtPct10 = enabled ? pct10 : 1000;

  // Zusätzlich Entlade-Limit als grobe §14a-Annäherung
  if (enabled) {
    uint16_t batPct = pct10 / 10; // 1000 → 100
    if (batPct < 10) batPct = 10;
    if (batPct > 100) batPct = 100;
    local.batMaxDischargePct = batPct;
  } else {
    local.batMaxDischargePct = 100;
  }

  ConfigStore::get().apcEnabled = local.apcEnabled;
  ConfigStore::get().apcPwrLmtPct10 = local.apcPwrLmtPct10;
  ConfigStore::get().batMaxDischargePct = local.batMaxDischargePct;

  bool ok = applyApcLimit();
  ok &= applyBatteryLimits();
  return ok;
}

} // namespace HitControl
