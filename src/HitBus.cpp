#include "HitBus.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#ifndef PIN_RS485_TX
#define PIN_RS485_TX 17
#endif
#ifndef PIN_RS485_RX
#define PIN_RS485_RX 18
#endif

namespace {
AppConfig local;
SemaphoreHandle_t lock = nullptr;
constexpr uint32_t TIMEOUT_MS = 1500;

// ---- TCP: eine Verbindung, nur bei Fehler neu -------------------------------
WiFiClient conn;
char connHost[64] = "";
uint16_t connPort = 0;
uint16_t nextTid = 1;

bool tcpConnect() {
  if (conn.connected() && connPort == local.hitPort && strcmp(connHost, local.hitHost) == 0) return true;
  conn.stop();
  if (!local.hitHost[0]) return false;
  if (!conn.connect(local.hitHost, local.hitPort, int32_t(TIMEOUT_MS))) return false;
  conn.setNoDelay(true);
  conn.setTimeout(TIMEOUT_MS);
  strlcpy(connHost, local.hitHost, sizeof(connHost));
  connPort = local.hitPort;
  return true;
}

bool tcpTransact(uint8_t unit, const uint8_t *pdu, size_t len, uint8_t *resp, size_t respMax, size_t &respLen) {
  for (int attempt = 0; attempt < 2; attempt++) {
    if (!tcpConnect()) return false;
    while (conn.available()) conn.read();
    const uint16_t tid = nextTid++;
    uint8_t hdr[7] = {uint8_t(tid >> 8), uint8_t(tid), 0, 0, uint8_t((len + 1) >> 8), uint8_t(len + 1), unit};
    if (conn.write(hdr, 7) == 7 && conn.write(pdu, len) == len) {
      uint8_t rh[7];
      if (conn.readBytes(rh, 7) == 7 && rh[0] == hdr[0] && rh[1] == hdr[1]) {
        size_t n = ((size_t(rh[4]) << 8) | rh[5]);
        if (n >= 2 && n - 1 <= respMax && conn.readBytes(resp, n - 1) == n - 1) {
          respLen = n - 1;
          return true;
        }
      }
    }
    conn.stop(); // halbe Antwort oder vom HIT getrennt: einmal neu verbinden
  }
  return false;
}

// ---- RTU über RS485 ---------------------------------------------------------
bool rtuStarted = false;
uint32_t rtuBaud = 0;
uint32_t lastFrameUs = 0;

uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xFFFF;
  while (n--) {
    c ^= *p++;
    for (int i = 0; i < 8; i++) c = (c & 1) ? (c >> 1) ^ 0xA001 : c >> 1;
  }
  return c;
}

void rtuBegin() {
  if (rtuStarted && rtuBaud == local.hitRtuBaud) return;
  Serial1.end();
  Serial1.begin(local.hitRtuBaud, SERIAL_8N1, PIN_RS485_RX, PIN_RS485_TX);
  Serial1.setTimeout(TIMEOUT_MS);
  rtuStarted = true;
  rtuBaud = local.hitRtuBaud;
}

size_t rtuRead(uint8_t *buf, size_t n) { return Serial1.readBytes(buf, n); }

bool rtuTransact(uint8_t unit, const uint8_t *pdu, size_t len, uint8_t *resp, size_t respMax, size_t &respLen) {
  rtuBegin();
  if (len + 3 > 260) return false;
  uint8_t frame[260];
  frame[0] = unit;
  memcpy(frame + 1, pdu, len);
  uint16_t c = crc16(frame, len + 1);
  frame[len + 1] = c & 0xFF;
  frame[len + 2] = c >> 8;

  // ≥3,5 Zeichen Pause zwischen Frames
  const uint32_t gapUs = max<uint32_t>(1750, 38500000UL / max<uint32_t>(local.hitRtuBaud, 1200));
  while (micros() - lastFrameUs < gapUs) delayMicroseconds(100);
  while (Serial1.available()) Serial1.read();
  Serial1.write(frame, len + 3);
  Serial1.flush();

  uint8_t buf[260];
  size_t got = rtuRead(buf, 2);
  lastFrameUs = micros();
  if (got != 2 || buf[0] != unit) return false;
  size_t body;
  const uint8_t fc = buf[1];
  if (fc & 0x80) {
    body = 1;
  } else if (fc <= 4) {
    if (rtuRead(buf + 2, 1) != 1) return false;
    got = 3;
    body = buf[2];
  } else {
    body = 4; // FC5/6/15/16: Adresse + Wert/Anzahl
  }
  size_t need = body + 2; // + CRC
  if (got + need > sizeof(buf) || rtuRead(buf + got, need) != need) return false;
  got += need;
  lastFrameUs = micros();
  uint16_t rc = crc16(buf, got - 2);
  if ((rc & 0xFF) != buf[got - 2] || (rc >> 8) != buf[got - 1]) return false;
  respLen = got - 3; // ohne Unit und CRC
  if (respLen > respMax) return false;
  memcpy(resp, buf + 1, respLen);
  return true;
}

bool readRegs(uint8_t fc, uint16_t addr, uint16_t qty, uint16_t *out) {
  if (qty == 0 || qty > 125) return false;
  uint8_t pdu[5] = {fc, uint8_t(addr >> 8), uint8_t(addr), uint8_t(qty >> 8), uint8_t(qty)};
  uint8_t resp[256];
  size_t n = 0;
  if (!HitBus::transact(local.hitUnitId, pdu, 5, resp, sizeof(resp), n)) return false;
  if (resp[0] != fc || n < 2 || resp[1] != qty * 2 || n < size_t(2 + qty * 2)) return false;
  for (uint16_t i = 0; i < qty; i++) out[i] = (uint16_t(resp[2 + i * 2]) << 8) | resp[3 + i * 2];
  return true;
}
} // namespace

namespace HitBus {

void begin(const AppConfig &cfg) {
  local = cfg;
  if (!lock) lock = xSemaphoreCreateMutex();
}

void applyConfig(const AppConfig &cfg) {
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  const bool linkChanged = cfg.hitMode != local.hitMode || cfg.hitPort != local.hitPort ||
                           strcmp(cfg.hitHost, local.hitHost) != 0 || cfg.demoMode != local.demoMode;
  local = cfg;
  if (linkChanged) conn.stop();
  if (lock) xSemaphoreGive(lock);
}

bool forbidden(const uint8_t *pdu, size_t pduLen) {
  if (pduLen < 3 || pdu[0] != 6) return false;
  const uint16_t addr = (uint16_t(pdu[1]) << 8) | pdu[2];
  return addr >= SAFETY_FIRST && addr <= SAFETY_LAST;
}

bool transact(uint8_t unit, const uint8_t *pdu, size_t pduLen, uint8_t *resp, size_t respMax, size_t &respLen) {
  if (local.demoMode || !lock || forbidden(pdu, pduLen)) return false;
  if (xSemaphoreTake(lock, pdMS_TO_TICKS(4000)) != pdTRUE) return false;
  bool ok = local.hitMode == HitLinkMode::RtuRs485 ? rtuTransact(unit, pdu, pduLen, resp, respMax, respLen)
                                              : tcpTransact(unit, pdu, pduLen, resp, respMax, respLen);
  xSemaphoreGive(lock);
  return ok;
}

bool readInput(uint16_t addr, uint16_t qty, uint16_t *out) { return readRegs(4, addr, qty, out); }
bool readHolding(uint16_t addr, uint16_t qty, uint16_t *out) { return readRegs(3, addr, qty, out); }

bool writeSingle(uint16_t addr, uint16_t value) {
  uint8_t pdu[5] = {6, uint8_t(addr >> 8), uint8_t(addr), uint8_t(value >> 8), uint8_t(value)};
  uint8_t resp[8];
  size_t n = 0;
  return transact(local.hitUnitId, pdu, 5, resp, sizeof(resp), n) && n >= 5 && resp[0] == 6;
}

bool writeMulti(uint16_t addr, uint16_t qty, const uint16_t *values) {
  if (qty == 0 || qty > 60) return false;
  uint8_t pdu[6 + 120] = {16, uint8_t(addr >> 8), uint8_t(addr), uint8_t(qty >> 8), uint8_t(qty), uint8_t(qty * 2)};
  for (uint16_t i = 0; i < qty; i++) {
    pdu[6 + i * 2] = values[i] >> 8;
    pdu[7 + i * 2] = values[i] & 0xFF;
  }
  uint8_t resp[8];
  size_t n = 0;
  return transact(local.hitUnitId, pdu, 6 + qty * 2, resp, sizeof(resp), n) && n >= 5 && resp[0] == 16;
}

void close() {
  if (lock) xSemaphoreTake(lock, portMAX_DELAY);
  conn.stop();
  if (lock) xSemaphoreGive(lock);
}

} // namespace HitBus
