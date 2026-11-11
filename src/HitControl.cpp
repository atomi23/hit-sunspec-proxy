#include "HitControl.h"
#include "AppConfig.h"
#include "HitBus.h"
#include "LiveData.h"

namespace {
AppConfig local;
String err;
bool started = false;

bool wr(uint16_t addr, uint16_t val) {
  bool ok = HitBus::writeSingle(addr, val);
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
  ok &= wr(306, c * 10); // Battery Max Charge Power, % ×10 seit V2.8
  ok &= wr(307, d * 10); // Battery Max Discharge Power, % ×10
  ok &= wr(308, mx);     // Max SoC, % ×1
  ok &= wr(309, mn);     // Min SoC, % ×1
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
  // 279/280 liegen im Netzschutz-Bereich: nur FC16, FC6 würde dort die Werksvorgaben laden
  uint16_t lim = clampU(local.apcPwrLmtPct10, 0, 1000);
  const uint16_t vals[2] = {uint16_t(local.apcEnabled ? 1 : 0), lim};
  bool ok = HitBus::writeMulti(279, 2, vals);
  if (!ok) err = "Write APC 279/280 fail";
  delay(80);
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
  // SunSpec WMaxLimPct = Wirkleistungsgrenze des Wechselrichters → HIT APC
  local.apcEnabled = enabled;
  local.apcPwrLmtPct10 = enabled ? pct10 : 1000;
  ConfigStore::get().apcEnabled = local.apcEnabled;
  ConfigStore::get().apcPwrLmtPct10 = local.apcPwrLmtPct10;
  return applyApcLimit();
}

} // namespace HitControl
