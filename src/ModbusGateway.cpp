#include "ModbusGateway.h"
#include "HitBus.h"
#include "LiveData.h"
#include "DebugLog.h"
#include <WiFi.h>

namespace {
AppConfig local;
WiFiServer server(502);
bool listening = false;
uint16_t listenPort = 0;
constexpr int MAX_CLIENTS = 4;
WiFiClient clients[MAX_CLIENTS];

void reply(WiFiClient &c, const uint8_t *mbap, const uint8_t *pdu, size_t len) {
  uint8_t out[7 + 256];
  memcpy(out, mbap, 4);
  out[4] = uint8_t((len + 1) >> 8);
  out[5] = uint8_t(len + 1);
  out[6] = mbap[6];
  memcpy(out + 7, pdu, len);
  c.write(out, 7 + len);
}

void exception(WiFiClient &c, const uint8_t *mbap, uint8_t fc, uint8_t code) {
  uint8_t pdu[2] = {uint8_t(fc | 0x80), code};
  reply(c, mbap, pdu, 2);
}

bool isWrite(uint8_t fc) { return fc == 5 || fc == 6 || fc == 15 || fc == 16; }

void handle(WiFiClient &c) {
  uint8_t mbap[7];
  if (c.readBytes(mbap, 7) != 7) { c.stop(); return; }
  const size_t len = (size_t(mbap[4]) << 8) | mbap[5];
  if (mbap[2] || mbap[3] || len < 2 || len > 254) { c.stop(); return; }
  uint8_t pdu[254];
  if (c.readBytes(pdu, len - 1) != len - 1) { c.stop(); return; }
  const size_t pduLen = len - 1;
  const uint8_t fc = pdu[0];

  if (local.demoMode) { exception(c, mbap, fc, 0x0B); return; }
  if (!(fc == 3 || fc == 4 || isWrite(fc))) { exception(c, mbap, fc, 0x01); return; }
  if (isWrite(fc)) {
    const uint16_t addr = pduLen >= 3 ? (uint16_t(pdu[1]) << 8) | pdu[2] : 0;
    if (HitBus::forbidden(pdu, pduLen)) {
      DebugLog::add(fc, addr, 0, "Gateway: FC6 auf Netzschutz 101-299 gesperrt");
      exception(c, mbap, fc, 0x01);
      return;
    }
    if (!local.hitWriteEnabled) {
      DebugLog::add(fc, addr, 0, "Gateway-Write abgelehnt (Schreiben aus)");
      exception(c, mbap, fc, 0x01);
      return;
    }
    DebugLog::add(fc, addr, pduLen >= 5 ? (uint16_t(pdu[3]) << 8) | pdu[4] : 0, "Gateway-Write an HIT");
  }

  uint8_t resp[256];
  size_t respLen = 0;
  const uint8_t unit = mbap[6] ? mbap[6] : local.hitUnitId;
  if (!HitBus::transact(unit, pdu, pduLen, resp, sizeof(resp), respLen)) {
    exception(c, mbap, fc, 0x0B); // Gateway: Zielgerät antwortet nicht
    return;
  }
  reply(c, mbap, resp, respLen);
}

void stopAll() {
  for (auto &c : clients) c.stop();
  if (listening) server.end();
  listening = false;
  gLive.gatewayClients = 0;
}

volatile bool pending = false;

void applyListen() {
  if (!local.gatewayEnabled || (listening && listenPort != local.gatewayPort)) stopAll();
  if (local.gatewayEnabled && !listening) {
    server = WiFiServer(local.gatewayPort);
    server.begin();
    listening = true;
    listenPort = local.gatewayPort;
  }
}
} // namespace

namespace ModbusGateway {

void begin(const AppConfig &cfg) {
  local = cfg;
  applyListen();
}

// kommt aus dem Web-Task: Server erst im loop() umstellen
void applyConfig(const AppConfig &cfg) {
  local = cfg;
  pending = true;
}

void loop() {
  if (pending) {
    pending = false;
    applyListen();
  }
  if (!listening) return;
  WiFiClient incoming = server.accept();
  if (incoming) {
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (!clients[i] || !clients[i].connected()) { slot = i; break; }
    }
    if (slot < 0) { clients[0].stop(); slot = 0; }
    incoming.setTimeout(500);
    incoming.setNoDelay(true);
    clients[slot] = incoming;
  }
  uint8_t n = 0;
  for (auto &c : clients) {
    if (!c) continue;
    if (!c.connected()) { c.stop(); continue; }
    n++;
    int guard = 4; // jede Anfrage geht an den HIT, Display und Web sollen nicht verhungern
    while (guard-- > 0 && c.available() >= 8) handle(c);
  }
  gLive.gatewayClients = n;
}

} // namespace ModbusGateway
