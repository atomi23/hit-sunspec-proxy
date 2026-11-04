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

struct IngestSnap {
  bool valid = false;
  int32_t pvW = 0, batW = 0, gridW = 0;
  uint16_t soc = 0;
  double energyPvKwh = 0, energyImpKwh = 0, energyExpKwh = 0;
  uint32_t atMs = 0;
} ingest;

String topicState() { return String(local.mqttBase) + "/state"; }
String topicAvail() { return String(local.mqttBase) + "/status"; }
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
  publishDiscoveryOne("tas_w", "Zähler Leistung", "W", "power", "measurement", "tasmota_w");
  publishDiscoveryOne("tas_in", "Zähler Bezug", "kWh", "energy", "total_increasing", "tasmota_in_kwh");
  publishDiscoveryOne("tas_out", "Zähler Einspeisung", "kWh", "energy", "total_increasing", "tasmota_out_kwh");
  publishDiscoveryOne("src", "Datenquelle", "", "", "", "data_origin");
  discoverySent = true;
}

void onMessage(char *topic, byte *payload, unsigned int len) {
  if (!local.mqttIngestEnabled) return;
  if (local.mqttIngestTopic[0] && strcmp(topic, local.mqttIngestTopic) != 0) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload, len);
  if (err) return;

  auto takeI = [&](const char *a, const char *b = nullptr) -> int32_t {
    if (!doc[a].isNull()) return doc[a].as<int32_t>();
    if (b && !doc[b].isNull()) return doc[b].as<int32_t>();
    return INT32_MIN;
  };
  auto takeF = [&](const char *a, const char *b = nullptr) -> double {
    if (!doc[a].isNull()) return doc[a].as<double>();
    if (b && !doc[b].isNull()) return doc[b].as<double>();
    return NAN;
  };

  int32_t pv = takeI("pv_w", "pvW");
  int32_t bat = takeI("bat_w", "batW");
  int32_t grid = takeI("grid_w", "gridW");
  int32_t soc = takeI("soc", "battery_soc");
  double ePv = takeF("energy_pv_kwh", "energyPvKwh");
  double eImp = takeF("energy_imp_kwh", "energyImpKwh");
  double eExp = takeF("energy_exp_kwh", "energyExpKwh");

  if (pv == INT32_MIN && bat == INT32_MIN && soc == INT32_MIN) return;

  if (pv != INT32_MIN) ingest.pvW = pv;
  if (bat != INT32_MIN) ingest.batW = bat;
  if (grid != INT32_MIN) ingest.gridW = grid;
  if (soc != INT32_MIN) {
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;
    ingest.soc = uint16_t(soc);
  }
  if (!isnan(ePv)) ingest.energyPvKwh = ePv;
  if (!isnan(eImp)) ingest.energyImpKwh = eImp;
  if (!isnan(eExp)) ingest.energyExpKwh = eExp;
  ingest.valid = true;
  ingest.atMs = millis();
  gLive.mqttIngestOk = true;
  gLive.lastMqttIngestMs = ingest.atMs;
}

bool ensureConnected() {
  if (!local.mqttEnabled && !local.mqttIngestEnabled) {
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
  mqtt.setBufferSize(2048);
  mqtt.setCallback(onMessage);
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
    if (local.mqttEnabled) {
      mqtt.publish(topicAvail().c_str(), "online", true);
      discoverySent = false;
      sendDiscovery();
    }
    if (local.mqttIngestEnabled && local.mqttIngestTopic[0]) {
      mqtt.subscribe(local.mqttIngestTopic);
    }
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
  ingest = {};
}

void applyConfig(const AppConfig &cfg) {
  if (mqtt.connected()) mqtt.disconnect();
  local = cfg;
  discoverySent = false;
  nextPub = 0;
  nextReconnect = 0;
  gLive.mqttOk = false;
  ingest = {};
  gLive.mqttIngestOk = false;
}

void publishNow() {
  if (!local.mqttEnabled) return;
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
  doc["cloud_ok"] = gLive.cloudOk;
  doc["data_origin"] = dataOriginLabel(gLive.dataOrigin);
  doc["ip"] = gLive.ipShown;

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(topicState().c_str(), payload.c_str(), false);
}

bool ingestFresh(uint32_t maxAgeMs) {
  if (!ingest.valid || !ingest.atMs) return false;
  return (millis() - ingest.atMs) <= maxAgeMs;
}

void applyIngestToLive() {
  if (!ingest.valid) return;
  gLive.pvW = ingest.pvW;
  gLive.batW = ingest.batW;
  gLive.gridW = ingest.gridW;
  gLive.soc = ingest.soc;
  gLive.energyPvKwh = ingest.energyPvKwh;
  gLive.energyImpKwh = ingest.energyImpKwh;
  gLive.energyExpKwh = ingest.energyExpKwh;
  gLive.mqttIngestOk = true;
  gLive.lastMqttIngestMs = ingest.atMs;
}

void loop() {
  if (!started || (!local.mqttEnabled && !local.mqttIngestEnabled)) {
    gLive.mqttOk = false;
    return;
  }
  if (!ensureConnected()) return;
  mqtt.loop();
  if (local.mqttEnabled && millis() >= nextPub) {
    nextPub = millis() + max<uint32_t>(2000, local.mqttPublishMs);
    publishNow();
  }
  if (ingest.valid && ingest.atMs &&
      (millis() - ingest.atMs) > max<uint32_t>(local.mqttIngestMaxAgeMs, 1000)) {
    gLive.mqttIngestOk = false;
  }
}

} // namespace MqttHa
