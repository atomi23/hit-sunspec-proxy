#include "Watchdog.h"
#include "LiveData.h"
#include "AppConfig.h"
#include <esp_task_wdt.h>

namespace {
bool ready = false;
uint32_t noSourceSince = 0;
bool hadSource = false;

#if ESP_IDF_VERSION_MAJOR >= 5
bool initTwdt(uint32_t timeoutSec) {
  esp_task_wdt_config_t cfg = {
      .timeout_ms = timeoutSec * 1000U,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  // Schon initialisiert? dann reconfigure
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) {
    if (esp_task_wdt_init(&cfg) != ESP_OK) return false;
  }
  if (esp_task_wdt_add(NULL) != ESP_OK) {
    // ggf. schon subscribed
  }
  return true;
}
#else
bool initTwdt(uint32_t timeoutSec) {
  esp_task_wdt_init(timeoutSec, true);
  esp_task_wdt_add(NULL);
  return true;
}
#endif
} // namespace

namespace Watchdog {

void begin(uint32_t timeoutSec) {
  if (timeoutSec < 15) timeoutSec = 15;
  ready = initTwdt(timeoutSec);
  noSourceSince = millis();
  hadSource = false;
  Serial.printf("Watchdog: TWDT %lus %s\n", (unsigned long)timeoutSec, ready ? "ok" : "fail");
}

void feed() {
  if (ready) esp_task_wdt_reset();
}

void checkSoft(uint32_t maxNoSourceMs) {
  auto &cfg = ConfigStore::get();
  if (cfg.demoMode) {
    hadSource = true;
    noSourceSince = millis();
    return;
  }

  const bool ok = (gLive.dataOrigin == DataOrigin::Native ||
                   gLive.dataOrigin == DataOrigin::Mqtt ||
                   gLive.dataOrigin == DataOrigin::Cloud);

  if (ok) {
    hadSource = true;
    noSourceSince = millis();
    return;
  }

  // Erst zählen, wenn wir schon mal Daten hatten (nicht beim ersten Boot ohne Netz)
  if (!hadSource) {
    noSourceSince = millis();
    return;
  }

  if (millis() - noSourceSince > maxNoSourceMs) {
    Serial.println("Watchdog soft: lange keine Quelle — restart");
    delay(200);
    ESP.restart();
  }
}

} // namespace Watchdog
