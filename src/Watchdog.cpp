#include "Watchdog.h"
#include "LiveData.h"
#include "AppConfig.h"
#include <esp_task_wdt.h>
#include <esp_system.h>

namespace {
bool ready = false;
uint32_t noSourceSince = 0;
bool hadSource = false;

constexpr uint32_t STAGE_MAGIC = 0x57444731; // "WDG1"
RTC_NOINIT_ATTR uint32_t stageMagic;
RTC_NOINIT_ATTR char stageName[24];
char hung[24] = "";

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
  const esp_reset_reason_t why = esp_reset_reason();
  if (stageMagic == STAGE_MAGIC && (why == ESP_RST_TASK_WDT || why == ESP_RST_INT_WDT || why == ESP_RST_WDT ||
                                    why == ESP_RST_PANIC)) {
    stageName[sizeof(stageName) - 1] = '\0';
    strlcpy(hung, stageName, sizeof(hung));
  }
  stage("Start");
  if (timeoutSec < 15) timeoutSec = 15;
  ready = initTwdt(timeoutSec);
  noSourceSince = millis();
  hadSource = false;
  Serial.printf("Watchdog: TWDT %lus %s\n", (unsigned long)timeoutSec, ready ? "ok" : "fail");
}

void feed() {
  if (ready) esp_task_wdt_reset();
}

void stage(const char *name) {
  strlcpy(stageName, name, sizeof(stageName));
  stageMagic = STAGE_MAGIC;
}

const char *hungIn() { return hung; }

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

const char *lastResetReason(bool en) {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return en ? "power on" : "Stromzufuhr";
    case ESP_RST_SW: return en ? "software restart" : "Neustart per Software";
    case ESP_RST_PANIC: return en ? "crash" : "Absturz";
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT: return en ? "watchdog" : "Watchdog";
    case ESP_RST_BROWNOUT: return en ? "brownout" : "Unterspannung";
    case ESP_RST_EXT: return en ? "reset pin" : "Reset-Taste";
    case ESP_RST_USB: return en ? "USB" : "USB";
    case ESP_RST_DEEPSLEEP: return en ? "deep sleep" : "Tiefschlaf";
    default: return en ? "unknown" : "unbekannt";
  }
}

} // namespace Watchdog
