#include "LiveData.h"
#include "AppConfig.h"
#include <cstring>
#include <cmath>

// Füllt SunSpec GEN24-ähnliche Holding-Register (wie Node-RED consolinno_fn_map)
  // Optional: Model 203 AC-Meter (3ph) aus Tasmota-Summe + HIT-Phasen (tasmotaMeterSpoof)

namespace SunSpecMap {

static void putStr(uint16_t *regs, int off, const char *s, int words) {
  char buf[64];
  memset(buf, 0, sizeof(buf));
  strncpy(buf, s ? s : "", words * 2);
  for (int i = 0; i < words; i++) {
    regs[off + i] = (uint16_t(buf[i * 2]) << 8) | uint16_t(buf[i * 2 + 1]);
  }
}

static uint16_t sfNeg(int n) { return uint16_t(0x10000 + n); }
static uint16_t sn16(int32_t n) {
  if (n < 0) return uint16_t(n + 0x10000);
  return uint16_t(n & 0xffff);
}

static void putU32(uint16_t *regs, int off, uint32_t v) {
  regs[off] = (v >> 16) & 0xffff;
  regs[off + 1] = v & 0xffff;
}

void build(uint16_t *regs, size_t n, const LiveData &live, const AppConfig &cfg) {
  if (n < 512) return;
  memset(regs, 0, n * sizeof(uint16_t));

  int32_t pv = live.pvW;
  int32_t bat = live.batW;
  if (cfg.invertPowerSign) {
    pv = -pv;
    bat = -bat;
  }
  const int32_t batRaw = live.batW;
  const int32_t chaW = batRaw < -10 ? labs(batRaw) : 0;
  const int32_t disW = batRaw > 10 ? batRaw : 0;
  const int32_t pvAbs = labs(pv);
  int32_t pvMppt = max(0, int(pvAbs - chaW - disW));
  if (pv < 0) pvMppt = -pvMppt;
  const int32_t acW = pv + bat;

  float v1 = live.v1, v2 = live.v2, v3 = live.v3;
  if (v1 < 50) v1 = 230;
  if (v2 < 50) v2 = 230;
  if (v3 < 50) v3 = 230;
  float i1 = live.i1, i2 = live.i2, i3 = live.i3;

  uint32_t wh = 0;
  if (live.energyPvKwh > 0.1) {
    wh = uint32_t(llround(live.energyPvKwh * 1000.0));
  }
  const uint16_t whHi = (wh >> 16) & 0xffff;
  const uint16_t whLo = wh & 0xffff;
  uint16_t wReg = uint16_t(acW & 0xffff);
  if (acW < 0) wReg = uint16_t(acW + 0x10000);

  const uint16_t aTotal = uint16_t(lround((fabsf(i1) + fabsf(i2) + fabsf(i3)) * 100));
  const uint16_t a1 = uint16_t(lround(fabsf(i1) * 100));
  const uint16_t a2 = uint16_t(lround(fabsf(i2) * 100));
  const uint16_t a3 = uint16_t(lround(fabsf(i3) * 100));
  const uint16_t vv1 = uint16_t(lround(v1 * 10));
  const uint16_t vv2 = uint16_t(lround(v2 * 10));
  const uint16_t vv3 = uint16_t(lround(v3 * 10));
  const uint16_t st = (pvAbs > 10 || labs(batRaw) > 10) ? 4 : 1;
  uint16_t chaSt = 6;
  uint16_t soc = live.soc;
  if (soc > 100) soc = 100;
  if (soc >= 99) chaSt = 5;
  else if (soc <= 1) chaSt = 2;
  else if (batRaw > 50) chaSt = 3;
  else if (batRaw < -50) chaSt = 4;

  const int WMAX = 20000;
  const int batV = 400;
  const int batA = batV > 0 ? int(lround((fabs(double(bat)) / batV) * 10)) : 0;

  // Common model
  regs[0] = 0x5375; regs[1] = 0x6e53; // "SunS"
  regs[2] = 1; regs[3] = 66;
  putStr(regs, 4, "Fronius", 16);
  putStr(regs, 20, "Symo GEN24 20.0 Plus", 16);
  putStr(regs, 36, "Hybrid", 8);
  putStr(regs, 44, PROXY_VERSION, 8);
  putStr(regs, 52, "HIT-Proxy", 8);
  regs[68] = 1;

  // Model 103
  regs[70] = 103; regs[71] = 50;
  regs[72] = aTotal; regs[73] = a1; regs[74] = a2; regs[75] = a3; regs[76] = sfNeg(-2);
  regs[77] = uint16_t(lround(v1 * 10 * 1.7320508f));
  regs[78] = uint16_t(lround(v2 * 10 * 1.7320508f));
  regs[79] = uint16_t(lround(v3 * 10 * 1.7320508f));
  regs[80] = vv1; regs[81] = vv2; regs[82] = vv3; regs[83] = sfNeg(-1);
  regs[84] = wReg; regs[85] = 0;
  regs[86] = 500; regs[87] = sfNeg(-1);
  regs[88] = wReg; regs[89] = 0;
  regs[92] = 1000; regs[93] = sfNeg(-1);
  regs[94] = whHi; regs[95] = whLo; regs[96] = 0;
  regs[97] = uint16_t(lround(fabs(double(acW)) / max(v1, 1.0f) * 10));
  regs[98] = sfNeg(-1);
  regs[99] = 4000; regs[100] = sfNeg(-1);
  regs[101] = wReg; regs[102] = 0;
  regs[103] = 350;
  regs[104] = 0x8000; regs[105] = 0x8000; regs[106] = 0x8000; regs[107] = sfNeg(-1);
  regs[108] = st; regs[109] = st;

  regs[122] = 120; regs[123] = 26;
  regs[124] = 82;
  regs[125] = WMAX; regs[126] = 0;
  regs[127] = WMAX; regs[128] = 0;
  regs[129] = WMAX; regs[130] = WMAX;
  regs[131] = sn16(-WMAX); regs[132] = sn16(-WMAX);
  regs[134] = 320; regs[135] = sfNeg(-1);
  regs[136] = 800; regs[137] = 800; regs[138] = 800; regs[139] = 800; regs[140] = sfNeg(-3);
  regs[141] = 4300; regs[142] = 1;
  regs[145] = WMAX; regs[147] = WMAX;

  regs[150] = 121; regs[151] = 30;
  regs[152] = WMAX; regs[153] = 2300;
  regs[157] = WMAX; regs[158] = WMAX; regs[159] = WMAX;
  regs[160] = sn16(-WMAX); regs[161] = sn16(-WMAX);
  regs[163] = 800; regs[164] = 800; regs[165] = 800; regs[166] = 800;
  regs[173] = sfNeg(-1); regs[179] = sfNeg(-3);

  regs[182] = 122; regs[183] = 44;
  regs[184] = 7; regs[185] = 7; regs[186] = 1;

  // Model 123 Immediate Controls (Consolinno LPP/LPC)
  regs[228] = 123; regs[229] = 24;
  regs[232] = 1; // Conn
  regs[233] = live.wMaxLimPct ? live.wMaxLimPct : 10000; // WMaxLimPct SF-2
  regs[237] = live.wMaxLimEna ? 1 : 0;                   // WMaxLimPct_Ena
  regs[251] = sfNeg(-2); regs[252] = sfNeg(-3); regs[253] = sfNeg(-2);

  // MPPT model 160
  regs[254] = 160; regs[255] = 88;
  regs[256] = sfNeg(-1); regs[257] = sfNeg(-1);
  regs[262] = 4;

  auto mppt = [&](int base, int id, int dca, int dcv, int32_t dcw, const char *label) {
    regs[base] = id;
    putStr(regs, base + 1, label, 8);
    regs[base + 9] = sn16(dca);
    regs[base + 10] = sn16(dcv);
    regs[base + 11] = sn16(dcw);
    regs[base + 16] = (labs(dcw) > 0 || id <= 2) ? 4 : 1;
  };

  const int32_t pvHalf = pvMppt / 2;
  const int32_t pvRem = pvMppt - pvHalf;
  auto dcA = [](int32_t w) { return int(lround(fabs(double(w)) / 40.0 * 10)); };
  mppt(264, 1, dcA(pvHalf), 4000, pvHalf, "PV1");
  mppt(284, 2, dcA(pvRem), 4000, pvRem, "PV2");
  mppt(304, 3, chaW ? batA : 0, batV * 10, chaW, "StCha 3");
  mppt(324, 4, disW ? batA : 0, batV * 10, disW, "StDisCha 4");

  // Storage 124
  regs[344] = 124; regs[345] = 24;
  regs[346] = WMAX;
  regs[347] = 100; regs[348] = 100;
  regs[349] = 3;
  regs[351] = 500;
  regs[352] = soc * 100; regs[353] = soc * 100;
  regs[354] = batV * 10; regs[355] = chaSt;
  regs[356] = 10000; regs[357] = 10000;
  regs[365] = sfNeg(-2); regs[366] = sfNeg(-2); regs[369] = sfNeg(-2);

  // Optional SunSpec Model 203 (dreiphasiger Zähler). Tasmota E320 liefert nur die Summe —
  // Phasenleistung daher zu je ~1/3, Spannungen vom HIT.
  int next = 370;
  if (cfg.tasmotaMeterSpoof && live.tasmotaOk) {
    const int32_t mW = live.tasmotaPowerW;
    const int32_t w1 = mW / 3, w2 = mW / 3, w3 = mW - w1 - w2;
    const uint32_t whImp = uint32_t(llround(live.tasmotaTotalKwh * 1000.0));
    const uint32_t whExp = uint32_t(llround(live.tasmotaExportKwh * 1000.0));
    const float va = v1 > 50 ? v1 : 230.0f, vb = v2 > 50 ? v2 : 230.0f, vc = v3 > 50 ? v3 : 230.0f;
    auto amp = [](int32_t w, float v) {
      return uint16_t(lround(fabs(double(w)) / max(v, 1.0f) * 100.0)); // A_SF -2
    };
    const uint16_t a1m = amp(w1, va), a2m = amp(w2, vb), a3m = amp(w3, vc);

    regs[next + 0] = 203;
    regs[next + 1] = 105;
    regs[next + 2] = uint16_t(a1m + a2m + a3m); // A
    regs[next + 3] = a1m;                       // AphA
    regs[next + 4] = a2m;
    regs[next + 5] = a3m;
    regs[next + 6] = sfNeg(-2);                 // A_SF
    regs[next + 7] = uint16_t(lround(((va + vb + vc) / 3.0f) * 10)); // PhV avg
    regs[next + 8] = uint16_t(lround(va * 10));
    regs[next + 9] = uint16_t(lround(vb * 10));
    regs[next + 10] = uint16_t(lround(vc * 10));
    regs[next + 11] = sfNeg(-1);                // PhV_SF
    regs[next + 12] = uint16_t(lround(va * 10 * 1.732f)); // PPV approx
    regs[next + 13] = uint16_t(lround(va * 10 * 1.732f));
    regs[next + 14] = uint16_t(lround(vb * 10 * 1.732f));
    regs[next + 15] = uint16_t(lround(vc * 10 * 1.732f));
    regs[next + 16] = sfNeg(-1);                // PPV_SF
    regs[next + 17] = uint16_t(lround((live.freqHz > 40 ? live.freqHz : 50.0f) * 100));
    regs[next + 18] = sfNeg(-2);                // Hz_SF
    regs[next + 19] = sn16(mW);                 // W
    regs[next + 20] = sn16(w1);
    regs[next + 21] = sn16(w2);
    regs[next + 22] = sn16(w3);
    regs[next + 23] = 0;                        // W_SF
    regs[next + 24] = uint16_t(labs(mW));       // VA
    regs[next + 25] = uint16_t(labs(w1));
    regs[next + 26] = uint16_t(labs(w2));
    regs[next + 27] = uint16_t(labs(w3));
    regs[next + 28] = 0;
    // VAR… PF
    regs[next + 34] = 1000;
    regs[next + 35] = 1000;
    regs[next + 36] = 1000;
    regs[next + 37] = 1000;
    regs[next + 38] = sfNeg(-3);                // PF_SF
    const uint32_t whExp1 = whExp / 3, whExp2 = whExp / 3, whExp3 = whExp - whExp1 - whExp2;
    const uint32_t whImp1 = whImp / 3, whImp2 = whImp / 3, whImp3 = whImp - whImp1 - whImp2;
    putU32(regs, next + 39, whExp);             // TotWhExp
    putU32(regs, next + 41, whExp1);
    putU32(regs, next + 43, whExp2);
    putU32(regs, next + 45, whExp3);
    regs[next + 47] = 0;                        // TotWhExp_SF Wh
    putU32(regs, next + 48, whImp);             // TotWhImp
    putU32(regs, next + 50, whImp1);
    putU32(regs, next + 52, whImp2);
    putU32(regs, next + 54, whImp3);
    regs[next + 56] = 0;
    putStr(regs, next + 90, live.tasmotaSensorKey[0] ? live.tasmotaSensorKey : "Tasmota", 8);
    next += 107; // 2 header + 105
  }

  // End model
  if (next + 1 < (int)n) {
    regs[next] = 0xffff;
    regs[next + 1] = 0;
  }
}

} // namespace SunSpecMap
