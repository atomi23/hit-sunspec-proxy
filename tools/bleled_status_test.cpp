// Build: g++ -std=c++17 -Wall -Wextra -Werror -DHAS_BLE_LED -Itools/host_stubs -Iinclude -I.pio/libdeps/t-connect-pro/ArduinoJson/src tools/bleled_status_test.cpp src/BleLed.cpp -o /tmp/bleled_status_test
#include "BleLed.h"
#include "EventLog.h"
#include "LotusLantern.h"
#include <esp_gattc_api.h>
#include <freertos/FreeRTOS.h>
#include <cassert>
#include <initializer_list>

namespace {
AppConfig cfg;
void (*worker)(void *);
bool step;
struct BatchDone {};
struct Write { uint8_t opcode; esp_err_t result; };
const Write *nextWrite, *endWrites;
constexpr esp_err_t WRITE_ERROR = 1;
constexpr char ERROR_TEXT[] = "Befehl nicht gesendet (Bluetooth-Fehler 1)";
}

AppConfig &ConfigStore::get() { return cfg; }
void EventLog::add(char, const char *, const char *, ...) {}

int xTaskCreatePinnedToCore(void (*run)(void *), const char *, uint32_t, void *, int, TaskHandle_t *task, int) {
  worker = run;
  *task = &worker;
  return pdTRUE;
}

uint32_t ulTaskNotifyTake(int, TickType_t) {
  if (!step) throw BatchDone{};
  step = false;
  return 1;
}

esp_err_t esp_ble_gattc_write_char(uint8_t, uint16_t, uint16_t, uint16_t len, uint8_t *data, int writeType, int) {
  assert(nextWrite != endWrites);
  assert(writeType == ESP_GATT_WRITE_TYPE_NO_RSP);
  assert(len == LotusLantern::CMD_LEN && data[2] == nextWrite->opcode);
  return (nextWrite++)->result;
}

static bool replay(const char *label, std::initializer_list<Write> writes, int brightness, const char *error) {
  nextWrite = writes.begin();
  endWrites = writes.end();
  step = true;
  try {
    worker(nullptr);
  } catch (const BatchDone &) {}
  assert(nextWrite == endWrites);

  JsonDocument doc;
  BleLed::status(doc.to<JsonObject>());
  const char *rgb = doc["rgb"] | "-";
  const char *actualError = doc["error"] | "";
  const int actualBrightness = doc["bri"] | -1;
  const char *power = doc["on"].isNull() ? "-" : doc["on"].as<bool>() ? "ein" : "aus";
  const bool ok = strcmp(power, "ein") == 0 && strcmp(rgb, "#123456") == 0 &&
                  actualBrightness == brightness && strcmp(actualError, error) == 0;
  printf("%s: %s (on=%s, rgb=%s, bri=%d, error=\"%s\")\n",
         label, ok ? "OK" : "FAIL", power, rgb, actualBrightness, actualError);
  return ok;
}

int main() {
  cfg.bleLedEnabled = true;
  strlcpy(cfg.bleLedAddr, "02:00:00:00:00:01", sizeof(cfg.bleLedAddr));
  BleLed::begin(cfg);

  BleLed::setColor(0x12, 0x34, 0x56);
  BleLed::setBrightness(45);
  const bool partial = replay("Ein und Farbe akzeptiert, Helligkeit abgelehnt",
                              {{4, ESP_OK}, {5, ESP_OK}, {1, WRITE_ERROR}}, -1, ERROR_TEXT);
  BleLed::setColor(0x12, 0x34, 0x56);
  BleLed::setBrightness(60);
  const bool retry = replay("Alle Befehle akzeptiert", {{4, ESP_OK}, {5, ESP_OK}, {1, ESP_OK}}, 60, "");

  BleLed::setPower(false);
  const bool rejected = replay("Aus abgelehnt", {{4, WRITE_ERROR}}, 60, ERROR_TEXT);
  return partial && retry && rejected ? 0 : 1;
}
