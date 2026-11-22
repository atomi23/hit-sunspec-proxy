// Build: c++ -std=c++17 -Itools/host_stubs -Iinclude tools/hitbus_rtu_test.cpp -o hitbus_rtu_test
#include "../src/HitBus.cpp"

static AppConfig cfg;
AppConfig &ConfigStore::get() { return cfg; }

// Antwort des HIT ab Unit-ID; CRC wird angehängt, keep schneidet danach ab
static void reply(std::initializer_list<uint8_t> bytes, size_t keep = SIZE_MAX) {
  uint8_t f[64];
  size_t n = 0;
  for (uint8_t b : bytes) f[n++] = b;
  const uint16_t c = crc16(f, n);
  f[n++] = c & 0xFF;
  f[n++] = c >> 8;
  Serial1.reply.assign(f, f + min(n, keep));
}

static int failures = 0;

static void expect(const char *what, const char *err) {
  uint16_t v = 0;
  const bool ok = HitBus::readHolding(0, 1, &v);
  const bool pass = err ? !ok && strcmp(HitBus::lastError(), err) == 0 : ok && v == 0x1234;
  printf("%s: %s (Fehler: \"%s\")\n", what, pass ? "OK" : "FAIL", HitBus::lastError());
  if (!pass) failures++;
}

int main() {
  cfg.hitMode = HitLinkMode::RtuRs485;
  HitBus::begin(cfg);

  reply({1, 3, 2, 0x12, 0x34});
  expect("vollständige Antwort", nullptr);
  Serial1.reply.clear();
  expect("keine Antwort", "RS485: keine Antwort");
  reply({1, 3, 2, 0x12, 0x34}, 2);
  expect("Abbruch vor der Bytezahl", "RS485: Antwort unvollständig");
  Serial1.reply.clear();
  expect("keine Antwort", "RS485: keine Antwort");
  reply({1, 3, 2, 0x12, 0x34}, 4);
  expect("Abbruch nach der Bytezahl", "RS485: Antwort unvollständig");

  puts(failures ? "HitBus RTU: FAIL" : "HitBus RTU: OK");
  return failures ? 1 : 0;
}
