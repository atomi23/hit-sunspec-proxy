#include "MqttHa.h"
#include "LiveData.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "BatteryData.h"
#include "BatteryLogic.h"
#include <math.h>

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
  double energyPvKwh = NAN, energyImpKwh = NAN, energyExpKwh = NAN; // NAN = nicht geliefert
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
  // fehlt der Schlüssel (noch keine Daten), wird der Sensor "unbekannt" statt 0: eine 0 hielte
  // Home Assistant bei total_increasing für einen Zählerreset und zählte danach alles doppelt
  doc["val_tpl"] = String("{{ value_json.") + jsonKey + " | default(None) }}";
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

// Akku: eigenes HA-Gerät, über den Proxy angebunden
String topicBattery() { return String(local.mqttBase) + "/battery"; }
bool batDiscSent = false;
uint32_t batLastHit = 0, batLastCloud = 0;

void publishBatDisc(const char *component, const char *objId, const char *name, const char *unit,
                    const char *devClass, const char *stateClass, bool enabled = true) {
  JsonDocument doc;
  doc["name"] = name;
  doc["uniq_id"] = String(local.mqttBase) + "_akku_" + objId;
  doc["stat_t"] = topicBattery();
  doc["avty_t"] = topicAvail();
  doc["pl_avail"] = "online";
  doc["pl_not_avail"] = "offline";
  doc["val_tpl"] = String("{{ value_json.") + objId + " | default(None) }}";
  if (!strcmp(component, "binary_sensor")) {
    doc["pl_on"] = 1;
    doc["pl_off"] = 0;
  }
  if (unit && unit[0]) doc["unit_of_meas"] = unit;
  if (devClass && devClass[0]) doc["dev_cla"] = devClass;
  if (stateClass && stateClass[0]) doc["stat_cla"] = stateClass;
  if (!enabled) doc["en"] = false;
  JsonObject dev = doc["dev"].to<JsonObject>();
  dev["ids"][0] = String(local.mqttBase) + "_akku";
  dev["name"] = String(local.deviceName) + " Akku";
  dev["mf"] = "HIT-Proxy";
  dev["mdl"] = "Akkudaten";
  dev["via_device"] = local.mqttBase;
  String payload;
  serializeJson(doc, payload);
  mqtt.publish((String("homeassistant/") + component + "/" + local.mqttBase + "_akku_" + objId + "/config").c_str(),
               payload.c_str(), true);
}

void sendBatDiscovery() {
  publishBatDisc("sensor", "cell_temp_max", "Zelltemperatur max", "°C", "temperature", "measurement");
  publishBatDisc("sensor", "cell_temp_min", "Zelltemperatur min", "°C", "temperature", "measurement");
  publishBatDisc("sensor", "cell_v_max", "Zellspannung max", "V", "voltage", "measurement");
  publishBatDisc("sensor", "cell_v_min", "Zellspannung min", "V", "voltage", "measurement");
  publishBatDisc("sensor", "cell_spread_mv", "Zellspreizung", "mV", "voltage", "measurement");
  publishBatDisc("sensor", "charge_lim_a", "Max. Ladestrom", "A", "current", "measurement");
  publishBatDisc("sensor", "discharge_lim_a", "Max. Entladestrom", "A", "current", "measurement");
  publishBatDisc("sensor", "cycles", "Ladezyklen", "", "", "total_increasing");
  publishBatDisc("sensor", "remain_kwh", "Restenergie", "kWh", "energy_storage", "measurement");
  publishBatDisc("sensor", "cloud_soc", "SoC (Cloud)", "%", "battery", "measurement");
  publishBatDisc("sensor", "cloud_soh", "SoH (Cloud)", "%", "", "measurement");
  publishBatDisc("binary_sensor", "alarm", "Akku-Alarm", "", "problem", "");
#ifdef BOARD_HAS_PSRAM
  for (int i = 1; i <= BAT_MAX_CELLS; i++) {
    char id[10], nm[16];
    snprintf(id, sizeof(id), "cell_%02d", i);
    snprintf(nm, sizeof(nm), "Zelle %02d", i);
    publishBatDisc("sensor", id, nm, "V", "voltage", "measurement", false);
  }
#endif
  batDiscSent = true;
}

void putF(JsonDocument &d, const char *k, float v, int dec) {
  if (!isnan(v)) d[k] = serialized(String(v, dec));
}

void publishBattery() {
  if (!local.mqttEnabled || !local.batEnabled || !mqtt.connected()) return;
  if (local.mqttDiscovery && !batDiscSent) sendBatDiscovery();
  const BatteryInfo b = BatteryData::snapshot();
  if (b.hit.atMs == batLastHit && b.cloud.atMs == batLastCloud) return; // nichts Neues
  batLastHit = b.hit.atMs;
  batLastCloud = b.cloud.atMs;
  const uint32_t iv = uint32_t(max<uint16_t>(local.batCloudPollS, BAT_CLOUD_MIN_S)) * 1000UL;
  const BatteryMerged m = mergeBattery(b, millis(), iv);
  JsonDocument d;
  putF(d, "cell_temp_max", m.cellTempMax, 0);
  putF(d, "cell_temp_min", m.cellTempMin, 0);
  putF(d, "cell_v_max", m.cellVMax, 3);
  putF(d, "cell_v_min", m.cellVMin, 3);
  putF(d, "cell_spread_mv", m.cellDiffMv, 0);
  putF(d, "charge_lim_a", m.chargeLimA, 1);
  putF(d, "discharge_lim_a", m.dischargeLimA, 1);
  putF(d, "cycles", m.cycles, 0);
  putF(d, "remain_kwh", m.remainKwh, 2);
  if (m.alarm >= 0) d["alarm"] = m.alarm;
  if (b.cloud.valid) {
    putF(d, "cloud_soc", b.cloud.soc, 0);
    putF(d, "cloud_soh", b.cloud.soh, 0);
    for (uint8_t i = 0; i < b.cloud.cells; i++) {
      char id[10];
      snprintf(id, sizeof(id), "cell_%02u", unsigned(i + 1));
      putF(d, id, b.cloud.cellV[i], 3);
    }
  }
  if (m.srcCellV) d["source_cellv"] = m.srcCellV;
  if (m.srcTemp) d["source_temp"] = m.srcTemp;
  if (d.size() == 0) return;
  String payload;
  serializeJson(d, payload);
  mqtt.publish(topicBattery().c_str(), payload.c_str(), false);
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
  if (int32_t(millis() - nextReconnect) < 0) return false;
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
      batDiscSent = false;
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
  if (mqtt.connected()) {
    // sauberes Trennen löst das Testament nicht aus; sonst bliebe "online" stehen
    if (local.mqttEnabled) mqtt.publish(topicAvail().c_str(), "offline", true);
    mqtt.disconnect();
  }
  local = cfg;
  discoverySent = false;
  batDiscSent = false;
  batLastHit = batLastCloud = 0;
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
  // Zählerstände nur senden, wenn sie gelesen wurden (siehe val_tpl)
  if (gLive.energyPvKwh > 0) doc["energy_pv_kwh"] = round(gLive.energyPvKwh * 10.0) / 10.0;
  if (gLive.energyImpKwh > 0) doc["energy_imp_kwh"] = round(gLive.energyImpKwh * 10.0) / 10.0;
  if (gLive.energyExpKwh > 0) doc["energy_exp_kwh"] = round(gLive.energyExpKwh * 10.0) / 10.0;
  doc["tasmota_ok"] = gLive.tasmotaOk;
  if (gLive.tasmotaOk) {
    doc["tasmota_w"] = gLive.tasmotaPowerW;
    if (gLive.tasmotaTotalKwh > 0) doc["tasmota_in_kwh"] = round(gLive.tasmotaTotalKwh * 1000.0) / 1000.0;
    if (gLive.tasmotaExportKwh > 0) doc["tasmota_out_kwh"] = round(gLive.tasmotaExportKwh * 1000.0) / 1000.0;
  }
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
  if (!isnan(ingest.energyPvKwh)) gLive.energyPvKwh = ingest.energyPvKwh;
  if (!isnan(ingest.energyImpKwh)) gLive.energyImpKwh = ingest.energyImpKwh;
  if (!isnan(ingest.energyExpKwh)) gLive.energyExpKwh = ingest.energyExpKwh;
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
  if (local.mqttEnabled && int32_t(millis() - nextPub) >= 0) {
    nextPub = millis() + max<uint32_t>(2000, local.mqttPublishMs);
    publishNow();
    publishBattery();
  }
  if (ingest.valid && ingest.atMs &&
      (millis() - ingest.atMs) > max<uint32_t>(local.mqttIngestMaxAgeMs, 1000)) {
    gLive.mqttIngestOk = false;
  }
}

} // namespace MqttHa
