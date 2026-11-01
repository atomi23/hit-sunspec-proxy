#include "ModbusTcpRaw.h"
#include <WiFi.h>

namespace {
uint16_t nextTid = 1;

bool transact(const char *host, uint16_t port, const uint8_t *req, size_t reqLen,
              uint8_t *resp, size_t respNeed, uint32_t timeoutMs) {
  WiFiClient c;
  c.setTimeout(timeoutMs);
  if (!c.connect(host, port)) return false;
  if (c.write(req, reqLen) != reqLen) { c.stop(); return false; }
  size_t got = c.readBytes(resp, respNeed);
  c.stop();
  return got == respNeed;
}
} // namespace

namespace ModbusTcpRaw {

bool readInputRegs(const char *host, uint16_t port, uint8_t unit,
                   uint16_t addr, uint16_t qty, uint16_t *out, uint32_t timeoutMs) {
  uint8_t req[12];
  uint16_t tid = ++nextTid;
  req[0] = tid >> 8; req[1] = tid & 0xff;
  req[2] = 0; req[3] = 0;
  req[4] = 0; req[5] = 6;
  req[6] = unit;
  req[7] = 4;
  req[8] = addr >> 8; req[9] = addr & 0xff;
  req[10] = qty >> 8; req[11] = qty & 0xff;

  uint8_t resp[9 + 250];
  size_t need = 9 + qty * 2;
  if (need > sizeof(resp)) return false;
  if (!transact(host, port, req, 12, resp, need, timeoutMs)) return false;
  if (resp[7] != 4 || resp[8] != qty * 2) return false;
  for (uint16_t i = 0; i < qty; i++) {
    out[i] = (uint16_t(resp[9 + i * 2]) << 8) | resp[10 + i * 2];
  }
  return true;
}

bool readHoldingRegs(const char *host, uint16_t port, uint8_t unit,
                     uint16_t addr, uint16_t qty, uint16_t *out, uint32_t timeoutMs) {
  uint8_t req[12];
  uint16_t tid = ++nextTid;
  req[0] = tid >> 8; req[1] = tid & 0xff;
  req[2] = 0; req[3] = 0;
  req[4] = 0; req[5] = 6;
  req[6] = unit;
  req[7] = 3;
  req[8] = addr >> 8; req[9] = addr & 0xff;
  req[10] = qty >> 8; req[11] = qty & 0xff;

  uint8_t resp[9 + 250];
  size_t need = 9 + qty * 2;
  if (need > sizeof(resp)) return false;
  if (!transact(host, port, req, 12, resp, need, timeoutMs)) return false;
  if (resp[7] != 3 || resp[8] != qty * 2) return false;
  for (uint16_t i = 0; i < qty; i++) {
    out[i] = (uint16_t(resp[9 + i * 2]) << 8) | resp[10 + i * 2];
  }
  return true;
}

bool writeHoldingReg(const char *host, uint16_t port, uint8_t unit,
                     uint16_t addr, uint16_t value, uint32_t timeoutMs) {
  uint8_t req[12];
  uint16_t tid = ++nextTid;
  req[0] = tid >> 8; req[1] = tid & 0xff;
  req[2] = 0; req[3] = 0;
  req[4] = 0; req[5] = 6;
  req[6] = unit;
  req[7] = 6; // FC6
  req[8] = addr >> 8; req[9] = addr & 0xff;
  req[10] = value >> 8; req[11] = value & 0xff;

  uint8_t resp[12];
  if (!transact(host, port, req, 12, resp, 12, timeoutMs)) return false;
  return resp[7] == 6;
}

} // namespace ModbusTcpRaw
