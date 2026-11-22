#include "BleLed.h"
#include "EventLog.h"
#include "LotusLantern.h"

#ifdef HAS_BLE_LED
#include <BLEDevice.h>

namespace {
constexpr uint32_t IDLE_DISCONNECT_MS = 10000; // danach ist der Strip wieder für die App frei
constexpr uint32_t CONNECT_TIMEOUT_MS = 6000;
constexpr uint8_t SCAN_SECONDS = 5;
constexpr size_t MAX_FOUND = 12;

struct Found {
  char addr[18];
  char name[24];
  uint8_t type;
  int8_t rssi;
};

struct Target {
  bool enabled = false;
  char addr[18] = "";
  uint8_t addrType = 0;
  char name[24] = "";
  uint8_t variant = 0;
};

// Nur der jeweils letzte Wunsch je Art zählt.
struct Pending {
  int8_t power = -1;
  int16_t bri = -1;
  bool hasColor = false;
  uint8_t r = 0, g = 0, b = 0;
  bool scan = false;
  bool any() const { return power >= 0 || bri >= 0 || hasColor; }
};

// Zuletzt an den Strip geschickte Werte; der Strip selbst meldet nichts zurück.
struct Sent {
  int8_t power = -1;
  int16_t bri = -1;
  bool hasColor = false;
  uint8_t r = 0, g = 0, b = 0;
};

SemaphoreHandle_t lock;
TaskHandle_t task;
Target target;
Pending pending;
Sent sent;
const char *connectErr = "";
bool targetChanged = false;
Found found[MAX_FOUND];
size_t foundN = 0;
const char *state = "aus";
const char *stateEn = "off";
char lastErr[96] = "";
bool scanning = false;

bool bleUp = false;
BLEClient *client = nullptr;
BLERemoteCharacteristic *tx = nullptr;

struct Guard {
  Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(lock); }
};

void setState(const char *de, const char *en) {
  Guard g;
  state = de;
  stateEn = en;
}

void fail(const char *msg) {
  {
    Guard g;
    strlcpy(lastErr, msg, sizeof(lastErr));
  }
  EventLog::add('W', "LED", "%s", msg);
}

void ensureBle() {
  if (bleUp) return;
  BLEDevice::init("");
  bleUp = true;
}

void drop() {
  tx = nullptr;
  if (client && client->isConnected()) client->disconnect();
}

void runScan() {
  drop();
  ensureBle();
  {
    Guard g;
    scanning = true;
    foundN = 0;
  }
  setState("sucht", "scanning");
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(80);
  BLEScanResults *res = scan->start(SCAN_SECONDS, false);
  {
    Guard g;
    for (int i = 0; res && i < res->getCount() && foundN < MAX_FOUND; i++) {
      BLEAdvertisedDevice d = res->getDevice(i);
      if (!d.haveName() || !LotusLantern::modelFor(d.getName().c_str())) continue;
      Found &f = found[foundN++];
      strlcpy(f.addr, d.getAddress().toString().c_str(), sizeof(f.addr));
      strlcpy(f.name, d.getName().c_str(), sizeof(f.name));
      f.type = d.getAddressType() == BLE_ADDR_TYPE_PUBLIC ? 0 : 1;
      f.rssi = int8_t(constrain(d.getRSSI(), -127, 0));
    }
    scanning = false;
  }
  scan->clearResults();
  setState("bereit", "idle");
}

bool connectStrip(const Target &t) {
  if (client && client->isConnected() && tx) return true;
  ensureBle();
  if (!client) client = BLEDevice::createClient();
  setState("verbindet", "connecting");
  tx = nullptr;
  if (!client->connect(BLEAddress(String(t.addr)), t.addrType ? BLE_ADDR_TYPE_RANDOM : BLE_ADDR_TYPE_PUBLIC,
                       CONNECT_TIMEOUT_MS)) {
    connectErr = ConfigStore::uiText("LED-Strip nicht erreichbar (ausgeschaltet, zu weit weg oder mit der App verbunden)",
                                     "LED strip not reachable (off, too far away or connected to the app)");
    return false;
  }
  static const char *const pairs[][2] = {{"fff0", "fff3"}, {"ffe0", "ffe1"}};
  const LotusLantern::Model *m = LotusLantern::modelFor(t.name);
  const int first = m && m->ffe1 ? 1 : 0;
  const char *chr = "";
  for (int k = 0; k < 2 && !tx; k++) {
    const char *const *p = pairs[(first + k) % 2];
    BLERemoteService *svc = client->getService(BLEUUID(p[0]));
    if (svc) tx = svc->getCharacteristic(BLEUUID(p[1]));
    chr = p[1];
  }
  if (!tx) {
    client->disconnect();
    connectErr = ConfigStore::uiText("verbunden, aber kein Schreibkanal fff3/ffe1: anderes Protokoll",
                                     "connected, but no write channel fff3/ffe1: different protocol");
    return false;
  }
  EventLog::add('I', "LED", ConfigStore::uiText("verbunden mit %s (%s), Schreibkanal %s",
                                                "connected to %s (%s), write channel %s"), t.name, t.addr, chr);
  setState("verbunden", "connected");
  return true;
}

// Jeder Befehl steht mit seinen Bytes im Ereignisprotokoll; eine Quittung vom Strip gibt es nicht.
bool write(const uint8_t *cmd, const char *what) {
  uint8_t buf[LotusLantern::CMD_LEN];
  memcpy(buf, cmd, sizeof(buf));
  if (!client->isConnected()) {
    fail(ConfigStore::uiText("Verbindung abgebrochen, Befehl nicht gesendet", "connection lost, command not sent"));
    return false;
  }
  tx->writeValue(buf, sizeof(buf), false);
  char hex[LotusLantern::CMD_LEN * 3 + 1];
  for (size_t i = 0; i < sizeof(buf); i++) snprintf(hex + i * 3, 4, "%02x ", buf[i]);
  hex[sizeof(hex) - 2] = 0;
  EventLog::add('I', "LED", ConfigStore::uiText("gesendet: %s (%s)", "sent: %s (%s)"), what, hex);
  vTaskDelay(pdMS_TO_TICKS(40));
  return true;
}

bool send(const Target &t, const Pending &p) {
  const auto v = LotusLantern::variantFor(t.name, LotusLantern::Variant(t.variant));
  uint8_t cmd[LotusLantern::CMD_LEN];
  char what[40];
  if (p.power >= 0) {
    LotusLantern::power(cmd, v, p.power);
    if (!write(cmd, p.power ? ConfigStore::uiText("ein", "on") : ConfigStore::uiText("aus", "off"))) return false;
  }
  if (p.hasColor) {
    LotusLantern::color(cmd, p.r, p.g, p.b);
    snprintf(what, sizeof(what), ConfigStore::uiText("Farbe #%02x%02x%02x", "color #%02x%02x%02x"), p.r, p.g, p.b);
    if (!write(cmd, what)) return false;
  }
  if (p.bri >= 0) {
    LotusLantern::brightness(cmd, uint8_t(p.bri));
    snprintf(what, sizeof(what), ConfigStore::uiText("Helligkeit %d %%", "brightness %d %%"), p.bri);
    if (!write(cmd, what)) return false;
  }
  Guard g;
  if (p.power >= 0) sent.power = p.power;
  if (p.hasColor) {
    sent.hasColor = true;
    sent.r = p.r;
    sent.g = p.g;
    sent.b = p.b;
  }
  if (p.bri >= 0) sent.bri = p.bri;
  return true;
}

void run(void *) {
  uint32_t lastUse = 0;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    Target t;
    Pending p;
    bool changed;
    {
      Guard g;
      t = target;
      p = pending;
      pending = Pending();
      changed = targetChanged;
      targetChanged = false;
    }
    if (changed) {
      {
        Guard g;
        sent = Sent();
      }
      drop();
      // Der Client merkt sich Dienste und Handles des alten Strips: für den neuen einen frischen anlegen.
      for (int i = 0; client && client->isConnected() && i < 20; i++) vTaskDelay(pdMS_TO_TICKS(100));
      delete client;
      client = nullptr;
    }
    if (p.scan) runScan();
    if (p.any()) {
      if (!t.addr[0]) {
        fail(ConfigStore::uiText("kein LED-Strip ausgewählt", "no LED strip selected"));
      } else {
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; attempt++) {
          ok = connectStrip(t);
          if (!ok) vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (ok) {
          lastUse = millis();
          if (send(t, p)) {
            Guard g;
            lastErr[0] = 0;
          }
        } else {
          fail(connectErr);
          setState("bereit", "idle");
        }
      }
    }
    if (client && client->isConnected()) {
      if (millis() - lastUse > IDLE_DISCONNECT_MS) {
        drop();
        setState("bereit", "idle");
      }
    } else if (tx) {
      tx = nullptr;
      setState("bereit", "idle");
    }
  }
}

// Aufrufer hält lock: Hauptschleife (KNX) und Web-Task dürfen nicht beide einen Task anlegen.
void wake() {
  if (!task) xTaskCreatePinnedToCore(run, "bleled", 8192, nullptr, 1, &task, 0);
  else xTaskNotifyGive(task);
}

bool usable() { return lock && target.enabled; }
} // namespace

namespace BleLed {
void begin(const AppConfig &cfg) {
  lock = xSemaphoreCreateMutex();
  applyConfig(cfg);
}

void applyConfig(const AppConfig &cfg) {
  if (!lock) return;
  Guard g;
  Target t;
  t.enabled = cfg.bleLedEnabled;
  strlcpy(t.addr, cfg.bleLedAddr, sizeof(t.addr));
  t.addrType = cfg.bleLedAddrType;
  strlcpy(t.name, cfg.bleLedName, sizeof(t.name));
  t.variant = cfg.bleLedVariant;
  if (memcmp(&t, &target, sizeof(t)) != 0) targetChanged = true;
  target = t;
  if (!t.enabled) pending = Pending();
  if (task) xTaskNotifyGive(task);
}

void loop() {}

void setPower(bool on) {
  if (!usable()) return;
  {
    Guard g;
    pending.power = on;
    if (!on) {
      pending.bri = -1;
      pending.hasColor = false;
    }
    wake();
  }
}

void setBrightness(uint8_t pct) {
  if (pct == 0) return setPower(false);
  if (!usable()) return;
  {
    Guard g;
    pending.power = 1;
    pending.bri = pct > 100 ? 100 : pct;
    wake();
  }
}

void setColor(uint8_t r, uint8_t g8, uint8_t b) {
  if (!(r | g8 | b)) return setPower(false);
  if (!usable()) return;
  {
    Guard g;
    pending.power = 1;
    pending.hasColor = true;
    pending.r = r;
    pending.g = g8;
    pending.b = b;
    wake();
  }
}

void startScan() {
  if (!lock) return;
  {
    Guard g;
    pending.scan = true;
    scanning = true;
    wake();
  }
}

void status(JsonObject out) {
  out["supported"] = true;
  if (!lock) return;
  Guard g;
  out["enabled"] = target.enabled;
  out["state"] = ConfigStore::uiText(target.enabled || task ? state : "aus", target.enabled || task ? stateEn : "off");
  out["scanning"] = scanning;
  out["error"] = lastErr;
  out["connected"] = tx != nullptr;
  out["name"] = target.name;
  out["addr"] = target.addr;
  if (sent.power >= 0) out["on"] = sent.power != 0;
  if (sent.bri >= 0) out["bri"] = sent.bri;
  if (sent.hasColor) {
    char rgb[8];
    snprintf(rgb, sizeof(rgb), "#%02x%02x%02x", sent.r, sent.g, sent.b);
    out["rgb"] = rgb;
  }
  JsonArray a = out["found"].to<JsonArray>();
  for (size_t i = 0; i < foundN; i++) {
    JsonObject o = a.add<JsonObject>();
    o["addr"] = found[i].addr;
    o["name"] = found[i].name;
    o["type"] = found[i].type;
    o["rssi"] = found[i].rssi;
  }
}
} // namespace BleLed

#else

namespace BleLed {
void begin(const AppConfig &) {}
void applyConfig(const AppConfig &) {}
void loop() {}
void setPower(bool) {}
void setBrightness(uint8_t) {}
void setColor(uint8_t, uint8_t, uint8_t) {}
void startScan() {}
void status(JsonObject out) { out["supported"] = false; }
} // namespace BleLed

#endif
