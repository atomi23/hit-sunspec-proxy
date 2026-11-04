#include "KnxIp.h"
#include "LiveData.h"
#include "KnxPoints.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <math.h>

namespace {
AppConfig local;
WiFiUDP udp;
bool started = false;
bool sockOk = false;
uint32_t nextPub = 0;
float lastVal[KNX_CH_COUNT];
bool lastInit[KNX_CH_COUNT];

bool parseGa(const char *s, uint16_t &out) {
  if (!s || !s[0]) return false;
  int a = -1, b = -1, c = -1;
  if (sscanf(s, "%d/%d/%d", &a, &b, &c) != 3) return false;
  if (a < 0 || a > 31 || b < 0 || b > 7 || c < 0 || c > 255) return false;
  out = uint16_t((a << 11) | (b << 8) | c);
  return true;
}

bool parsePa(const char *s, uint16_t &out) {
  if (!s || !s[0]) {
    out = 0xFFFE;
    return true;
  }
  int a = 15, b = 15, c = 254;
  if (sscanf(s, "%d.%d.%d", &a, &b, &c) != 3) return false;
  if (a < 0 || a > 15 || b < 0 || b > 15 || c < 0 || c > 255) return false;
  out = uint16_t((a << 12) | (b << 8) | c);
  return true;
}

void putF16be(uint8_t *p, uint16_t v) {
  p[0] = uint8_t(v >> 8);
  p[1] = uint8_t(v & 0xff);
}

void encodeF32be(uint8_t *out, float v) {
  union {
    float f;
    uint32_t u;
  } x;
  x.f = v;
  out[0] = uint8_t((x.u >> 24) & 0xff);
  out[1] = uint8_t((x.u >> 16) & 0xff);
  out[2] = uint8_t((x.u >> 8) & 0xff);
  out[3] = uint8_t(x.u & 0xff);
}

void encodeU32be(uint8_t *out, uint32_t v) {
  out[0] = uint8_t((v >> 24) & 0xff);
  out[1] = uint8_t((v >> 16) & 0xff);
  out[2] = uint8_t((v >> 8) & 0xff);
  out[3] = uint8_t(v & 0xff);
}

void encodeDpt9(uint8_t *out, float value) {
  if (value > 670760.0f) value = 670760.0f;
  if (value < -670760.0f) value = -670760.0f;
  int exp = 0;
  float v = value;
  while ((v > 2047.0f || v < -2048.0f) && exp < 15) {
    v *= 0.5f;
    exp++;
  }
  int mantissa = int(lroundf(v));
  if (mantissa > 2047) mantissa = 2047;
  if (mantissa < -2048) mantissa = -2048;
  uint16_t raw = uint16_t(((exp & 0x0F) << 11) | (mantissa & 0x07FF));
  if (mantissa < 0) raw |= 0x8000;
  putF16be(out, raw);
}

bool sendGroupWrite(uint16_t ga, const uint8_t *data, uint8_t dataLen) {
  if (!sockOk || dataLen == 0 || dataLen > 14) return false;
  uint16_t srcPa = 0xFFFE;
  parsePa(local.knxPa, srcPa);

  uint8_t cemi[32];
  uint8_t n = 0;
  cemi[n++] = 0x29;
  cemi[n++] = 0x00;
  cemi[n++] = 0xBC;
  cemi[n++] = 0xE0;
  putF16be(cemi + n, srcPa);
  n += 2;
  putF16be(cemi + n, ga);
  n += 2;
  cemi[n++] = uint8_t(1 + dataLen);
  cemi[n++] = 0x00;
  cemi[n++] = 0x80;
  memcpy(cemi + n, data, dataLen);
  n = uint8_t(n + dataLen);

  uint8_t pkt[64];
  uint16_t total = uint16_t(6 + n);
  pkt[0] = 0x06;
  pkt[1] = 0x10;
  pkt[2] = 0x05;
  pkt[3] = 0x30;
  pkt[4] = uint8_t(total >> 8);
  pkt[5] = uint8_t(total & 0xff);
  memcpy(pkt + 6, cemi, n);

  IPAddress mcast(224, 0, 23, 12);
  if (!udp.beginPacket(mcast, 3671)) return false;
  udp.write(pkt, total);
  return udp.endPacket() == 1;
}

bool encodeByDpt(uint16_t dpt, KnxValKind kind, float value, uint8_t *buf, uint8_t &len) {
  if (dpt == 5001) {
    float pct = value;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    buf[0] = uint8_t(lroundf(pct * 255.0f / 100.0f));
    len = 1;
    return true;
  }
  if (dpt == 9024) {
    encodeDpt9(buf, value / 1000.0f);
    len = 2;
    return true;
  }
  if (dpt == 9007 || (dpt >= 9000 && dpt < 10000)) {
    encodeDpt9(buf, value);
    len = 2;
    return true;
  }
  if (dpt == 13010 || dpt == 13001) {
    encodeU32be(buf, uint32_t(llround(double(value) * 1000.0))); // kWh→Wh
    len = 4;
    return true;
  }
  // 14.xxx float (W, V, A, Hz, kWh)
  if (kind == KnxValKind::EnergyKwh && dpt == 14019) {
    encodeF32be(buf, value);
    len = 4;
    return true;
  }
  encodeF32be(buf, value);
  len = 4;
  return true;
}

float readPoint(uint8_t id) {
  switch (id) {
    case KNX_PV_W: return float(gLive.pvW);
    case KNX_PV1_W: return float(gLive.pv1W);
    case KNX_PV1_V: return gLive.pv1V;
    case KNX_PV1_A: return gLive.pv1A;
    case KNX_PV2_W: return float(gLive.pv2W);
    case KNX_PV2_V: return gLive.pv2V;
    case KNX_PV2_A: return gLive.pv2A;
    case KNX_PV3_W: return float(gLive.pv3W);
    case KNX_PV3_V: return gLive.pv3V;
    case KNX_PV3_A: return gLive.pv3A;
    case KNX_PV4_W: return float(gLive.pv4W);
    case KNX_PV4_V: return gLive.pv4V;
    case KNX_PV4_A: return gLive.pv4A;
    case KNX_BAT_W: return float(gLive.batW);
    case KNX_BAT_V: return gLive.batV;
    case KNX_BAT_A: return gLive.batA;
    case KNX_SOC: return float(gLive.soc);
    case KNX_SOH: return float(gLive.soh);
    case KNX_GRID_W: return float(gLive.gridW);
    case KNX_GRID_W1: return float(gLive.gridW1);
    case KNX_GRID_W2: return float(gLive.gridW2);
    case KNX_GRID_W3: return float(gLive.gridW3);
    case KNX_V1: return gLive.v1;
    case KNX_V2: return gLive.v2;
    case KNX_V3: return gLive.v3;
    case KNX_I1: return gLive.i1;
    case KNX_I2: return gLive.i2;
    case KNX_I3: return gLive.i3;
    case KNX_FREQ: return gLive.freqHz;
    case KNX_EPV: return float(gLive.energyPvKwh);
    case KNX_EIMP: return float(gLive.energyImpKwh);
    case KNX_EEXP: return float(gLive.energyExpKwh);
    case KNX_TAS_W: return float(gLive.tasmotaPowerW);
    case KNX_TAS_IN: return float(gLive.tasmotaTotalKwh);
    case KNX_TAS_OUT: return float(gLive.tasmotaExportKwh);
    case KNX_LPP: return gLive.wMaxLimPct / 100.0f;
    default: return 0;
  }
}

bool significantChange(uint8_t id, float now) {
  if (!local.knxOnlyOnChange || !lastInit[id]) return true;
  const auto &m = knxMeta(id);
  float thr = 0.05f;
  if (m.kind == KnxValKind::PowerW) thr = float(local.knxMinDeltaW);
  else if (m.kind == KnxValKind::Percent || m.kind == KnxValKind::RatioPct) thr = 0.5f;
  else if (m.kind == KnxValKind::Volt) thr = 0.5f;
  else if (m.kind == KnxValKind::Amp) thr = 0.05f;
  else if (m.kind == KnxValKind::Hz) thr = 0.02f;
  else if (m.kind == KnxValKind::EnergyKwh) thr = 0.1f;
  return fabsf(now - lastVal[id]) >= thr;
}

bool ensureSock() {
  if (sockOk) return true;
  if (udp.beginMulticast(IPAddress(224, 0, 23, 12), 3671) || udp.begin(3671)) {
    sockOk = true;
    return true;
  }
  return false;
}

void publish() {
  if (!local.knxEnabled) {
    gLive.knxOk = false;
    return;
  }
  if (!ensureSock()) {
    gLive.knxOk = false;
    return;
  }

  bool any = false;
  for (uint8_t i = 0; i < KNX_CH_COUNT; i++) {
    const KnxCh &ch = local.knxCh[i];
    if (!ch.ena) continue;
    if ((i == KNX_TAS_W || i == KNX_TAS_IN || i == KNX_TAS_OUT) && !gLive.tasmotaOk) continue;

    uint16_t ga;
    if (!parseGa(ch.ga, ga)) continue;
    float v = readPoint(i);
    if (!significantChange(i, v)) continue;

    const auto &m = knxMeta(i);
    uint16_t dpt = ch.dpt ? ch.dpt : m.defaultDpt;
    uint8_t buf[4], len = 0;
    encodeByDpt(dpt, m.kind, v, buf, len);
    if (sendGroupWrite(ga, buf, len)) {
      lastVal[i] = v;
      lastInit[i] = true;
      any = true;
    }
  }
  gLive.knxOk = sockOk;
  if (any) gLive.lastKnxMs = millis();
}
} // namespace

namespace KnxIp {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  sockOk = false;
  nextPub = 0;
  for (int i = 0; i < KNX_CH_COUNT; i++) lastInit[i] = false;
}

void applyConfig(const AppConfig &cfg) {
  udp.stop();
  sockOk = false;
  local = cfg;
  nextPub = 0;
  gLive.knxOk = false;
  for (int i = 0; i < KNX_CH_COUNT; i++) lastInit[i] = false;
}

void publishNow() { publish(); }
bool ok() { return gLive.knxOk; }

void loop() {
  if (!started || !local.knxEnabled) {
    gLive.knxOk = false;
    return;
  }
  if (millis() < nextPub) return;
  nextPub = millis() + max<uint32_t>(2000, local.knxPublishMs);
  publish();
}

} // namespace KnxIp
