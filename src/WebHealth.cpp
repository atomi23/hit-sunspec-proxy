#include "WebHealth.h"
#include "AppConfig.h"
#include "EventLog.h"
#include "OtaUpdate.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
constexpr uint32_t REASON_MAGIC = 0x57484C31; // "WHL1"
RTC_NOINIT_ATTR uint32_t reasonMagic;
RTC_NOINIT_ATTR uint8_t reasonCode;

constexpr uint32_t FIRST_CHECK_MS = 90000; // Netzwerk, HTTPS-Zertifikat und erste Abfragen erst fertig werden lassen
constexpr uint32_t PERIOD_MS = 30000;
constexpr int MAX_FAILS = 4; // vier Fehlschläge in Folge, also rund zwei Minuten ohne Antwort

const char *reasonText(WebHealth::Reason r) {
  switch (r) {
    case WebHealth::Reason::Http:
      return ConfigStore::uiText("Weboberfläche (Port 80) antwortete nicht", "Web UI (port 80) stopped responding");
    case WebHealth::Reason::Https:
      return ConfigStore::uiText("HTTPS-Proxy (Port 443) hing", "HTTPS proxy (port 443) was stuck");
    default: return "?";
  }
}

// Port 80 über die Loopback-Adresse: dort leitet die Weboberfläche nicht auf HTTPS um
bool probeHttp() {
  WiFiClient c;
  if (!c.connect(IPAddress(127, 0, 0, 1), 80, 3000)) return false;
  c.setTimeout(5000);
  static const char req[] = "GET /api/ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
  c.write(reinterpret_cast<const uint8_t *>(req), sizeof(req) - 1);
  const String status = c.readStringUntil('\n');
  c.stop();
  return status.startsWith("HTTP/1.");
}

void task(void *) {
  vTaskDelay(pdMS_TO_TICKS(FIRST_CHECK_MS));
  int fails = 0;
  for (;;) {
    if (OtaUpdate::inProgress()) {
      fails = 0;
    } else if (probeHttp()) {
      if (fails) EventLog::add('I', "Web", ConfigStore::uiText("Selbsttest Port 80 wieder ok", "Port 80 self-test ok again"));
      fails = 0;
    } else {
      fails++;
      EventLog::add('W', "Web", ConfigStore::uiText("Selbsttest Port 80 ohne Antwort (%d/%d), RAM frei %u",
                                                    "Port 80 self-test got no answer (%d/%d), free RAM %u"),
                    fails, MAX_FAILS, unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
      if (fails >= MAX_FAILS) WebHealth::restart(WebHealth::Reason::Http);
    }
    vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
  }
}
} // namespace

namespace WebHealth {

void begin() {
  if (reasonMagic == REASON_MAGIC && esp_reset_reason() == ESP_RST_SW && reasonCode != uint8_t(Reason::None))
    EventLog::add('W', "Web", ConfigStore::uiText("Neustart durch Selbsttest: %s", "Restarted by self-test: %s"),
                  reasonText(Reason(reasonCode)));
  reasonMagic = 0;
  reasonCode = 0;
  xTaskCreate(task, "webhealth", 4096, nullptr, 1, nullptr);
}

void restart(Reason why) {
  reasonCode = uint8_t(why);
  reasonMagic = REASON_MAGIC;
  EventLog::add('E', "Web", ConfigStore::uiText("%s, starte neu", "%s; restarting"), reasonText(why));
  delay(300);
  ESP.restart();
}

} // namespace WebHealth
