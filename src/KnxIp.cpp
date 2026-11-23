#include "KnxIp.h"
#include "LiveData.h"
#include "KnxPoints.h"
#include "KnxDisplayTelegram.h"
#include "StatusDisplay.h"
#include "BleLed.h"
#include "BatteryData.h"
#include "BatteryLogic.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <math.h>

namespace {
AppConfig local;
WiFiUDP udp;
bool started = false;
bool sockOk = false;
bool multicastJoined = false;
uint16_t displayGa = 0;
uint16_t ownPa = 0xFFFE;
uint16_t ledGa[3] = {}; // schalten, Helligkeit, Farbe
uint32_t nextJoin = 0;
char displayRxIp[sizeof(gLive.ipShown)] = "";
uint32_t nextPub = 0;
float lastVal[KNX_CH_COUNT];
bool lastInit[KNX_CH_COUNT];
BatteryMerged bat;   // einmal je publish() berechnet
bool batOn = false;

bool parseGa(const char *s, uint16_t &out) {
  if (!s || !s[0]) return false;
  int a = -1, b = -1, c = -1;
  int end = 0;
  if (sscanf(s, "%2d/%1d/%3d%n", &a, &b, &c, &end) != 3 || !end || s[end]) return false;
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
  // KNX DPT9: decoded = 0.01 * mantissa * 2^exp → Mantisse in 0,01-Schritten
  if (value > 670760.0f) value = 670760.0f;
  if (value < -670760.0f) value = -670760.0f;
  float v = value * 100.0f;
  int exp = 0;
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
  if (!sockOk || dataLen > 14) return false;

  uint8_t cemi[32];
  uint8_t n = 0;
  cemi[n++] = 0x29;
  cemi[n++] = 0x00;
  cemi[n++] = 0xBC;
  cemi[n++] = 0xE0;
  putF16be(cemi + n, ownPa);
  n += 2;
  putF16be(cemi + n, ga);
  n += 2;
  cemi[n++] = uint8_t(1 + dataLen);
  cemi[n++] = 0x00;
  cemi[n++] = dataLen ? 0x80 : uint8_t(0x80 | (data[0] & 0x3F));
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
  if (dpt == 1005 || dpt / 1000 == 1) {
    buf[0] = value != 0 ? 1 : 0;
    len = 0; // 1 Bit im APCI-Byte
    return true;
  }
  if (dpt == 12001) {
    encodeU32be(buf, value <= 0 ? 0u : uint32_t(lroundf(value)));
    len = 4;
    return true;
  }
  if (dpt == 13013) {
    encodeU32be(buf, uint32_t(int32_t(lroundf(value)))); // kWh, I32
    len = 4;
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
    case KNX_AKKU_TMAX: return bat.cellTempMax;
    case KNX_AKKU_TMIN: return bat.cellTempMin;
    case KNX_AKKU_VMAX: return bat.cellVMax * 1000.0f;
    case KNX_AKKU_VMIN: return bat.cellVMin * 1000.0f;
    case KNX_AKKU_VDIFF: return bat.cellDiffMv;
    case KNX_AKKU_CYCLES: return bat.cycles;
    case KNX_AKKU_REMAIN: return bat.remainKwh;
    case KNX_AKKU_ALARM: return bat.alarm < 0 ? NAN : float(bat.alarm);
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
  else if (m.kind == KnxValKind::Temp) thr = 0.5f;
  else if (m.kind == KnxValKind::MilliVolt) thr = 5.0f;
  else if (m.kind == KnxValKind::Count) thr = 1.0f;
  else if (m.kind == KnxValKind::Flag) thr = 0.5f;
  return fabsf(now - lastVal[id]) >= thr;
}

bool ensureSock() {
  if (sockOk) return true;
  multicastJoined = udp.beginMulticast(IPAddress(224, 0, 23, 12), 3671);
  sockOk = multicastJoined || udp.begin(3671);
  return sockOk;
}

void readDisplaySwitch(uint32_t now) {
  if (strcmp(displayRxIp, gLive.ipShown) != 0) {
    strlcpy(displayRxIp, gLive.ipShown, sizeof(displayRxIp));
    multicastJoined = false;
    nextJoin = now;
  }
  if (!multicastJoined && int32_t(now - nextJoin) >= 0) {
    nextJoin = now + 2000;
    sockOk = false;
    ensureSock();
  }
  if (!multicastJoined) return;

  // Die Hauptschleife auch bei vielen Telegrammen nicht blockieren.
  uint8_t packet[KnxDisplayTelegram::MAX_SIZE];
  for (int i = 0; i < 8; ++i) {
    const int len = udp.parsePacket();
    if (len <= 0) break;
    uint16_t ga;
    uint8_t value, n;
    const uint8_t *data;
    if (len <= int(sizeof(packet)) && udp.read(packet, len) == len &&
        KnxDisplayTelegram::readWrite(packet, len, ownPa, ga, value, data, n) && ga) {
      bool hit = true;
      if (KnxDisplayTelegram::matchesDisplaySwitch(ga, displayGa, value, n)) StatusDisplay::setKnxSwitch(value);
      else if (ga == ledGa[0] && n == 0 && value <= 1) BleLed::setPower(value);
      else if (ga == ledGa[1] && n == 1) BleLed::setBrightness(KnxDisplayTelegram::scalingPercent(data[0]));
      else if (ga == ledGa[2] && n == 3) BleLed::setColor(data[0], data[1], data[2]);
      else hit = false;
      if (hit) gLive.lastKnxMs = now;
    }
    udp.clear();
  }
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

  batOn = local.batEnabled;
  if (batOn) {
    const uint32_t iv = uint32_t(max<uint16_t>(local.batCloudPollS, BAT_CLOUD_MIN_S)) * 1000UL;
    bat = mergeBattery(BatteryData::snapshot(), millis(), iv);
  }
  bool any = false;
  for (uint8_t i = 0; i < KNX_DISPLAY_SWITCH; i++) {
    const KnxCh &ch = local.knxCh[i];
    if (!ch.ena) continue;
    if ((i == KNX_TAS_W || i == KNX_TAS_IN || i == KNX_TAS_OUT) && !gLive.tasmotaOk) continue;

    uint16_t ga;
    if (!parseGa(ch.ga, ga)) continue;
    if (i >= KNX_AKKU_TMAX && !batOn) continue;
    float v = readPoint(i);
    if (isnan(v)) continue; // Wert gerade nicht vorhanden
    if (!significantChange(i, v)) continue;

    const auto &m = knxMeta(i);
    uint16_t dpt = uint16_t(ch.dpt ? ch.dpt : m.defaultDpt);
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
  started = true;
  applyConfig(cfg);
}

void applyConfig(const AppConfig &cfg) {
  udp.stop();
  sockOk = false;
  multicastJoined = false;
  local = cfg;
  ownPa = 0xFFFE;
  parsePa(cfg.knxPa, ownPa);
  uint16_t ga = 0;
  if (HAS_DISPLAY && cfg.knxEnabled && cfg.knxCh[KNX_DISPLAY_SWITCH].ena)
    parseGa(cfg.knxCh[KNX_DISPLAY_SWITCH].ga, ga);
  if (ga != displayGa) StatusDisplay::clearKnxSwitch();
  displayGa = ga;
  for (int k = 0; k < 3; k++) {
    const KnxCh &ch = cfg.knxCh[KNX_LED_SWITCH + k];
    ledGa[k] = 0;
    if (BleLed::SUPPORTED && cfg.knxEnabled && cfg.bleLedEnabled && ch.ena) parseGa(ch.ga, ledGa[k]);
  }
  nextJoin = nextPub = millis();
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
  const uint32_t now = millis();
  if (displayGa || ledGa[0] || ledGa[1] || ledGa[2]) readDisplaySwitch(now);
  if (int32_t(now - nextPub) < 0) return;
  nextPub = now + max<uint32_t>(2000, local.knxPublishMs);
  publish();
}

} // namespace KnxIp
