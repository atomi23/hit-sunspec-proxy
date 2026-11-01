#include "MqttHa.h"
#include "LiveData.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

namespace {
AppConfig local;
WiFiClient wifi;
PubSubClient mqtt(wifi);
uint32_t nextPub = 0;
uint32_t nextReconnect = 0;
bool started = false;
bool discoverySent = false;

String topicState() {
  return String(local.mqttBase) + "/state";
}
String topicAvail() {
  return String(local.mqttBase) + "/status";
}
String topicDisc(const char *objId) {
  return String("homeassistant/sensor/") + local.mqttBase + "_" + objId + "/config";
}

void publishDiscoveryOne(const char *objId, const char *name, const char *unit,
                         const char *deviceClass, const char *stateClass, const char *jsonKey) {
  JsonDocument doc;
  doc["name"] = name;
  doc["uniq_id"] = String(local.mqttBase) + "_" + objId;
  doc["stat_t"] = topicState();
  doc["avty_t"] = topicAvail();
  doc["pl_avail"] = "online";
  doc["pl_not_avail"] = "offline";
  doc["val_tpl"] = String("{{ value_json.") + jsonKey + " }}";
  if (unit && unit[0]) doc["unit_of_meas"] = unit;
  if (deviceClass && deviceClass[0]) doc["dev_cla"] = deviceClass;
  if (stateClass && stateClass[0]) doc["stat_cla"] = stateClass;
  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"][0] = local.mqttBase;
  dev["name"] = local.deviceName;
  dev["mf"] = "HIT-Proxy";
  dev["mdl"] = "SunSpec Proxy";
  dev["sw"] = PROXY_VERSION;

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(topicDisc(objId).c_str(), payload.c_str(), true);
}

void sendDiscovery() {
  if (!local.mqttDiscovery) return;
  publishDiscoveryOne("pv_w", "PV Leistung", "W", "power", "measurement", "pv_w");
  publishDiscoveryOne("bat_w", "Batterie Leistung", "W", "power", "measurement", "bat_w");
  publishDiscoveryOne("grid_w", "Netz Leistung HIT", "W", "power", "measurement", "grid_w");
  publishDiscoveryOne("soc", "Batterie SoC", "%", "battery", "measurement", "soc");
  publishDiscoveryOne("energy_pv", "PV Energie", "kWh", "energy", "total_increasing", "energy_pv_kwh");
  publishDiscoveryOne("tas_w", "Zaehler Leistung", "W", "power", "measurement", "tasmota_w");
  publishDiscoveryOne("tas_in", "Zaehler Bezug", "kWh", "energy", "total_increasing", "tasmota_in_kwh");
  publishDiscoveryOne("tas_out", "Zaehler Einspeisung", "kWh", "energy", "total_increasing", "tasmota_out_kwh");
  discoverySent = true;
}

bool ensureConnected() {
  if (!local.mqttEnabled) {
    gLive.mqttOk = false;
    return false;
  }
  if (mqtt.connected()) {
    gLive.mqttOk = true;
    return true;
  }
  if (millis() < nextReconnect) return false;
  nextReconnect = millis() + 5000;

  mqtt.setServer(local.mqttHost, local.mqttPort);
  mqtt.setBufferSize(1024);
  String cid = String(local.mqttBase) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok;
  if (local.mqttUser[0]) {
    ok = mqtt.connect(cid.c_str(), local.mqttUser, local.mqttPass,
                      topicAvail().c_str(), 0, true, "offline");
  } else {
    ok = mqtt.connect(cid.c_str(), topicAvail().c_str(), 0, true, "offline");
  }
  gLive.mqttOk = ok;
  if (ok) {
    mqtt.publish(topicAvail().c_str(), "online", true);
    discoverySent = false;
    sendDiscovery();
  }
  return ok;
}
} // namespace

namespace MqttHa {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  discoverySent = false;
  nextPub = 0;
  nextReconnect = 0;
}

void applyConfig(const AppConfig &cfg) {
  if (mqtt.connected()) mqtt.disconnect();
  local = cfg;
  discoverySent = false;
  nextPub = 0;
  nextReconnect = 0;
  gLive.mqttOk = false;
}

void publishNow() {
  if (!ensureConnected()) return;
  if (!discoverySent) sendDiscovery();

  JsonDocument doc;
  doc["pv_w"] = gLive.pvW;
  doc["bat_w"] = gLive.batW;
  doc["grid_w"] = gLive.gridW;
  doc["soc"] = gLive.soc;
  doc["energy_pv_kwh"] = round(gLive.energyPvKwh * 10.0) / 10.0;
  doc["energy_imp_kwh"] = round(gLive.energyImpKwh * 10.0) / 10.0;
  doc["energy_exp_kwh"] = round(gLive.energyExpKwh * 10.0) / 10.0;
  doc["tasmota_ok"] = gLive.tasmotaOk;
  doc["tasmota_w"] = gLive.tasmotaPowerW;
  doc["tasmota_in_kwh"] = round(gLive.tasmotaTotalKwh * 1000.0) / 1000.0;
  doc["tasmota_out_kwh"] = round(gLive.tasmotaExportKwh * 1000.0) / 1000.0;
  doc["hit_ok"] = gLive.hitOk;
  doc["ip"] = gLive.ipShown;

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(topicState().c_str(), payload.c_str(), false);
}

void loop() {
  if (!started || !local.mqttEnabled) {
    gLive.mqttOk = false;
    return;
  }
  if (!ensureConnected()) return;
  mqtt.loop();
  if (millis() >= nextPub) {
    nextPub = millis() + max<uint32_t>(2000, local.mqttPublishMs);
    publishNow();
  }
}

} // namespace MqttHa
