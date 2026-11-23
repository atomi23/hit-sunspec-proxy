// Build: c++ -std=c++17 -Wall -Wextra -Werror -Itools/host_stubs -Iinclude tools/client_eviction_test.cpp src/SunSpecServer.cpp src/ModbusGateway.cpp -o /tmp/client_eviction_test && /tmp/client_eviction_test
#include "DebugLog.h"
#include "HitBus.h"
#include "HitControl.h"
#include "ModbusGateway.h"
#include "SunSpecMap.h"
#include "SunSpecServer.h"
#include <WiFi.h>

AppConfig &ConfigStore::get() {
  static AppConfig cfg;
  return cfg;
}
void DebugLog::add(uint8_t, uint16_t, uint16_t, const char *) {}
bool HitBus::forbidden(const uint8_t *, size_t) { return false; }
bool HitBus::transact(uint8_t, const uint8_t *, size_t, uint8_t *, size_t, size_t &) { return false; }
bool HitControl::onSunSpecPowerLimit(uint16_t, bool) { return false; }
void SunSpecMap::build(uint16_t *, size_t, const LiveData &, const AppConfig &) {}

static int failures = 0;

static void expect(const char *server, const char *what, bool pass) {
  printf("%s – %s: %s\n", server, what, pass ? "OK" : "FAIL");
  if (!pass) failures++;
}

static WiFiClient connect(uint16_t port, void (*loop)()) {
  WiFiClient c{std::make_shared<WiFiClient::Socket>()};
  WiFiServer::incoming[port].push_back(c);
  loop();
  return c;
}

template <size_t N>
static void testEviction(const char *server, uint16_t port, void (*loop)()) {
  WiFiClient clients[N];
  for (auto &c : clients) {
    hostMillis += 100;
    c = connect(port, loop);
  }

  hostMillis += 100;
  clients[0].socket->rx = {0, 1, 0, 0, 0, 6, 1, 3, 0, 0, 0, 1};
  loop();
  expect(server, "Anfrage auf Slot 0 verarbeitet", clients[0].socket->bytesWritten > 0);

  hostMillis += 100;
  WiFiClient incoming = connect(port, loop);
  expect(server, "inaktivsten Client getrennt, aktiven Slot 0 behalten",
         clients[0].connected() && !clients[1].connected() && incoming.connected() &&
           std::all_of(clients + 2, clients + N, [](const WiFiClient &c) { return c.connected(); }));

  bool connected[N];
  for (size_t i = 0; i < N; i++) connected[i] = clients[i].connected();
  incoming.stop();
  hostMillis += 100;
  WiFiClient reused = connect(port, loop);
  bool kept = true;
  for (size_t i = 0; i < N; i++) kept &= clients[i].connected() == connected[i];
  expect(server, "freien Slot ohne weitere Trennung wiederverwendet", reused.connected() && kept);
}

int main() {
  AppConfig cfg;
  cfg.gatewayEnabled = true;
  SunSpecServer::begin(cfg);
  ModbusGateway::begin(cfg);
  testEviction<2>("SunSpec", cfg.sunspecPort, SunSpecServer::loop);
  testEviction<4>("Modbus-Gateway", cfg.gatewayPort, ModbusGateway::loop);
  puts(failures ? "Client-Verdrängung: FAIL" : "Client-Verdrängung: OK");
  return failures ? 1 : 0;
}
