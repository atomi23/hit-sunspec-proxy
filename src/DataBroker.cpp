#include "DataSource.h"
#include "AppConfig.h"
#include "LiveData.h"
#include "MqttHa.h"
#include "HoyCloud.h"

namespace DataBroker {

void begin() {}

void reconcile() {
  auto &cfg = ConfigStore::get();

  if (cfg.demoMode) {
    gLive.dataOrigin = DataOrigin::Demo;
    return;
  }

  const uint32_t now = millis();
  const bool hitFresh =
      gLive.hitOk && gLive.lastHitMs &&
      (now - gLive.lastHitMs) <= max<uint32_t>(cfg.hitPollMs * 3, 90000);

  if (hitFresh) {
    gLive.dataOrigin = DataOrigin::Native;
    return;
  }

  if (cfg.mqttIngestEnabled && MqttHa::ingestFresh(cfg.mqttIngestMaxAgeMs)) {
    MqttHa::applyIngestToLive();
    gLive.dataOrigin = DataOrigin::Mqtt;
    strlcpy(gLive.status, "MQTT Fallback", sizeof(gLive.status));
    return;
  }

  if (cfg.cloudEnabled) {
    if (!HoyCloud::fresh(cfg.cloudMaxAgeMs)) {
      HoyCloud::pollNow();
    }
    if (HoyCloud::fresh(cfg.cloudMaxAgeMs)) {
      gLive.dataOrigin = DataOrigin::Cloud;
      strlcpy(gLive.status, "Cloud Fallback", sizeof(gLive.status));
      return;
    }
  }

  if (gLive.dataOrigin == DataOrigin::Native || gLive.dataOrigin == DataOrigin::None) {
    gLive.dataOrigin = DataOrigin::None;
    if (!gLive.hitOk) strlcpy(gLive.status, "keine Quelle", sizeof(gLive.status));
  }
}

} // namespace DataBroker
