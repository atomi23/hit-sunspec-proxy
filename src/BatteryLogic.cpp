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
