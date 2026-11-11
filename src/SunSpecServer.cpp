#include "SunSpecServer.h"
#include "SunSpecMap.h"
#include "HitControl.h"
#include "DebugLog.h"
#include <WiFi.h>

namespace {
AppConfig local;
WiFiServer server(1502);
constexpr uint16_t REGS = 800; // wie holdingBufferSize in Node-RED
uint16_t holding[REGS];
uint32_t nextRebuild = 0;
bool started = false;
constexpr int MAX_CLIENTS = 2;
WiFiClient clients[MAX_CLIENTS];

// Gleiches Abbild ab 0 (wie Node-RED-Buffer) und auf den SunSpec-Standardbasen 40000/50000
int toLocal(uint32_t addr) {
  if (addr < REGS) return int(addr);
  if (addr >= 40000 && addr < 40000u + REGS) return int(addr - 40000);
  if (addr >= 50000 && addr < 50000u + REGS) return int(addr - 50000);
  return -1;
}

// Model 123 base in our map
constexpr uint16_t M123 = 228;
constexpr uint16_t OFF_WMAX = 5;     // WMaxLimPct
constexpr uint16_t OFF_WMAX_ENA = 9; // WMaxLimPct_Ena

void writeU16(uint8_t *p, uint16_t v) {
  p[0] = v >> 8;
  p[1] = v & 0xff;
}

void noteForAddr(uint16_t addr, uint16_t value, char *out, size_t n) {
  if (addr == M123 + OFF_WMAX) {
    snprintf(out, n, "M123 WMaxLimPct %.2f%%", value / 100.0f);
  } else if (addr == M123 + OFF_WMAX_ENA) {
    snprintf(out, n, "M123 WMaxLim Ena=%u", value);
  } else if (addr >= M123 && addr < M123 + 26) {
    snprintf(out, n, "M123 off+%u", unsigned(addr - M123));
  } else {
    snprintf(out, n, "Holding");
  }
}

void onHoldingWrite(uint16_t wireAddr, uint16_t value, uint8_t fc) {
  int idx = toLocal(wireAddr);
  if (idx < 0) return;
  uint16_t addr = uint16_t(idx);
  holding[addr] = value;

  char note[40];
  noteForAddr(addr, value, note, sizeof(note));
  DebugLog::add(fc, addr, value, note);

  if (addr == M123 + OFF_WMAX) {
    gLive.wMaxLimPct = value;
  } else if (addr == M123 + OFF_WMAX_ENA) {
    gLive.wMaxLimEna = value != 0;
  } else {
    return;
  }
  HitControl::onSunSpecPowerLimit(gLive.wMaxLimPct, gLive.wMaxLimEna);
}

void sendException(WiFiClient &c, const uint8_t *hdr, uint8_t fc, uint8_t code) {
  uint8_t resp[9];
  resp[0] = hdr[0]; resp[1] = hdr[1];
  resp[2] = 0; resp[3] = 0;
  writeU16(resp + 4, 3);
  resp[6] = hdr[6];
  resp[7] = fc | 0x80;
  resp[8] = code;
  c.write(resp, 9);
}

void handleClient(WiFiClient &c) {
  uint8_t hdr[8];
  if (c.readBytes(hdr, 8) != 8) return;
  uint16_t tid = (uint16_t(hdr[0]) << 8) | hdr[1];
  uint16_t len = (uint16_t(hdr[4]) << 8) | hdr[5];
  uint8_t unit = hdr[6];
  uint8_t fc = hdr[7];
  (void)unit;

  if (len < 2) return;
  int rem = int(len) - 2;
  if (rem < 0 || rem > 256) return;
  uint8_t pdu[256];
  if (rem > 0 && c.readBytes(pdu, rem) != rem) return;

  if (fc == 3 || fc == 4) {
    if (rem < 4) return;
    uint16_t addr = (uint16_t(pdu[0]) << 8) | pdu[1];
    uint16_t qty = (uint16_t(pdu[2]) << 8) | pdu[3];
    if (qty == 0 || qty > 125) { sendException(c, hdr, fc, 0x03); return; }
    int base = toLocal(addr);
    if (base < 0) { sendException(c, hdr, fc, 0x02); return; }
    uint8_t resp[9 + 250];
    writeU16(resp, tid);
    resp[2] = 0; resp[3] = 0;
    writeU16(resp + 4, 3 + qty * 2);
    resp[6] = hdr[6];
    resp[7] = fc;
    resp[8] = qty * 2;
    for (uint16_t i = 0; i < qty; i++) {
      uint16_t v = (base + i < REGS) ? holding[base + i] : 0;
      writeU16(resp + 9 + i * 2, v);
    }
    c.write(resp, 9 + qty * 2);
  } else if (fc == 6) {
    // Write single holding
    if (rem < 4) return;
    uint16_t addr = (uint16_t(pdu[0]) << 8) | pdu[1];
    uint16_t value = (uint16_t(pdu[2]) << 8) | pdu[3];
    onHoldingWrite(addr, value, 6);
    uint8_t resp[12];
    writeU16(resp, tid);
    resp[2] = 0; resp[3] = 0;
    writeU16(resp + 4, 6);
    resp[6] = hdr[6];
    resp[7] = 6;
    writeU16(resp + 8, addr);
    writeU16(resp + 10, value);
    c.write(resp, 12);
  } else if (fc == 16) {
    // Write multiple holdings
    if (rem < 5) return;
    uint16_t addr = (uint16_t(pdu[0]) << 8) | pdu[1];
    uint16_t qty = (uint16_t(pdu[2]) << 8) | pdu[3];
    uint8_t bc = pdu[4];
    if (qty == 0 || qty > 123 || bc != qty * 2 || rem < 5 + bc) return;
    for (uint16_t i = 0; i < qty; i++) {
      uint16_t value = (uint16_t(pdu[5 + i * 2]) << 8) | pdu[6 + i * 2];
      onHoldingWrite(addr + i, value, 16);
    }
    uint8_t resp[12];
    writeU16(resp, tid);
    resp[2] = 0; resp[3] = 0;
    writeU16(resp + 4, 6);
    resp[6] = hdr[6];
    resp[7] = 16;
    writeU16(resp + 8, addr);
    writeU16(resp + 10, qty);
    c.write(resp, 12);
  } else {
    sendException(c, hdr, fc, 0x01);
  }
}
} // namespace

namespace SunSpecServer {

void rebuildMap() {
  SunSpecMap::build(holding, REGS, gLive, local);
  // Control-Werte nach Rebuild wieder setzen
  holding[M123 + OFF_WMAX] = gLive.wMaxLimPct ? gLive.wMaxLimPct : 10000;
  holding[M123 + OFF_WMAX_ENA] = gLive.wMaxLimEna ? 1 : 0;
  gLive.lastMapMs = millis();
}

void begin(const AppConfig &cfg) {
  local = cfg;
  server = WiFiServer(cfg.sunspecPort);
  server.begin();
  rebuildMap();
  started = true;
  nextRebuild = 0;
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
  rebuildMap();
}

void loop() {
  if (!started) return;
  if (millis() >= nextRebuild) {
    nextRebuild = millis() + 2000;
    rebuildMap();
  }
  // HEMS hält die TCP-Verbindung offen und pollt darüber — nicht nach jeder Anfrage trennen
  WiFiClient incoming = server.accept();
  if (incoming) {
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients[i] || !clients[i].connected()) { slot = i; break; }
    }
    if (slot < 0) {
      clients[0].stop(); // ältesten Slot freigeben, z. B. nach Neustart des HEMS
      slot = 0;
    }
    incoming.setTimeout(300);
    incoming.setNoDelay(true);
    clients[slot] = incoming;
  }

  bool any = false;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    WiFiClient &c = clients[i];
    if (!c) continue;
    if (!c.connected()) { c.stop(); continue; }
    any = true;
    int guard = 8; // max. Anfragen pro Durchlauf, damit Display/Web nicht verhungern
    while (guard-- > 0 && c.available() >= 8) handleClient(c);
  }
  gLive.sunspecClient = any;
}

} // namespace SunSpecServer
