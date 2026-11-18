#include "BatteryCloud.h"
#include "LiveData.h"
#include "EventLog.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <time.h>

namespace {
BatteryCloudData work;
uint32_t nextPoll = 0;
uint8_t backoff = 1;
bool failing = false;
bool selftestDone = false;
uint32_t netUpAt = 0;

constexpr uint32_t SETUP_GAP_MS = 15000; // Gerät suchen, binden: kurze Abstände, danach das Intervall

BatteryCloudDriver *driverFor(const AppConfig &cfg) {
  switch (cfg.batCloud) {
    case BatteryCloudType::Dyness: return &dynessDriver();
    default: return nullptr;
  }
}

uint32_t intervalMs(const AppConfig &cfg) {
  const uint32_t s = max<uint32_t>(cfg.batCloudPollS, BAT_CLOUD_MIN_S) * backoff;
  return min<uint32_t>(s, 3600) * 1000UL;
}

void fail(const char *msg) {
  BatteryData::setCloudError(msg);
  if (!failing) EventLog::add('W', "Akku", "Akku-Cloud: %s", msg);
  failing = true;
}
} // namespace

namespace BatteryCloud {

void reset() {
  dynessDriver().reset();
  work = BatteryCloudData{};
  nextPoll = 0;
  backoff = 1;
  failing = false;
  BatteryData::updateCloud(work);
}

void step(const AppConfig &cfg) {
  if (!selftestDone) {
    selftestDone = true;
    Serial.printf("Dyness sign selftest: %s\n", dynessSignSelftest() ? "ok" : "FAIL");
  }
  if (!cfg.batEnabled) return;
  BatteryCloudDriver *drv = driverFor(cfg);
  if (!drv) return;

  if (!gLive.ethOk && WiFi.status() != WL_CONNECTED) {
    netUpAt = 0;
    return;
  }
  if (!netUpAt) netUpAt = millis() | 1;
  if (millis() - netUpAt < 5000) return;
  if (nextPoll && int32_t(millis() - nextPoll) < 0) return;

  if (!cfg.batCloudAppId[0] || !cfg.batCloudSecret[0]) {
    fail("App-ID und Secret fehlen");
    nextPoll = (millis() + 60000) | 1;
    return;
  }
  // die Signatur enthält die Uhrzeit; ohne NTP lehnt Dyness jede Anfrage ab
  if (time(nullptr) < 1700000000) {
    BatteryData::setCloudError("wartet auf Uhrzeit (NTP)");
    nextPoll = (millis() + 10000) | 1;
    return;
  }
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 70000) {
    nextPoll = (millis() + 3000) | 1;
    return;
  }

  String err;
  const CloudResult r = drv->poll(cfg, work, err);
  switch (r) {
    case CloudResult::Data:
      BatteryData::updateCloud(work);
      if (failing) EventLog::add('I', "Akku", "Akku-Cloud liefert wieder Daten");
      failing = false;
      nextPoll = (millis() + intervalMs(cfg)) | 1;
      break;
    case CloudResult::DataMore:
      BatteryData::updateCloud(work);
      failing = false;
      nextPoll = (millis() + SETUP_GAP_MS) | 1;
      break;
    case CloudResult::Busy:
      nextPoll = (millis() + SETUP_GAP_MS) | 1;
      break;
    case CloudResult::RateLimited: {
      if (backoff < 16) backoff *= 2;
      char msg[64];
      snprintf(msg, sizeof(msg), "Abrufgrenze erreicht, nächster Versuch in %lu s",
               (unsigned long)(intervalMs(cfg) / 1000));
      fail(msg);
      nextPoll = (millis() + intervalMs(cfg)) | 1;
      break;
    }
    default:
      fail(err.length() ? err.c_str() : "Abruf fehlgeschlagen");
      nextPoll = (millis() + intervalMs(cfg)) | 1;
      break;
  }
}

} // namespace BatteryCloud
