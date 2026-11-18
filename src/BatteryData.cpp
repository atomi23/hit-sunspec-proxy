#include "BatteryData.h"
#include "BatteryLogic.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

namespace {
BatteryInfo data;
SemaphoreHandle_t lock = nullptr;

struct Guard {
  Guard() { if (lock) xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { if (lock) xSemaphoreGive(lock); }
};

void put(JsonObject o, const char *k, float v, int dec = 1) {
  if (!isnan(v)) o[k] = serialized(String(v, dec));
}

uint32_t ageS(uint32_t atMs) { return atMs ? (millis() - atMs) / 1000 : 0; }
} // namespace

namespace BatteryData {

void begin() {
  if (!lock) lock = xSemaphoreCreateMutex();
}

BatteryInfo snapshot() {
  Guard g;
  return data;
}

void updateHit(const BatteryHit &h) {
  Guard g;
  if (h.valid) {
    data.hit = h;
    data.hitMissing = false;
  } else {
    data.hit.valid = false;
    data.hitMissing = true;
  }
}

void updatePacks(const BatteryPack *p, uint8_t n) {
  Guard g;
  data.packs = n > BAT_MAX_PACKS ? BAT_MAX_PACKS : n;
  for (uint8_t i = 0; i < data.packs; i++) data.pack[i] = p[i];
  data.packsAtMs = millis();
}

void updateCloud(const BatteryCloudData &c) {
  Guard g;
  data.cloud = c;
  data.cloudErr[0] = 0;
}

void setCloudError(const char *err) {
  Guard g;
  strlcpy(data.cloudErr, err ? err : "", sizeof(data.cloudErr));
  data.cloudErrAtMs = millis();
}

void toJson(JsonDocument &doc, const AppConfig &cfg) {
  const BatteryInfo b = snapshot();
  doc["enabled"] = cfg.batEnabled;
  doc["hitEnabled"] = cfg.batHitEnabled;
  doc["cloud"] = int(cfg.batCloud);

  JsonObject h = doc["hit"].to<JsonObject>();
  h["valid"] = b.hit.valid;
  h["missing"] = b.hitMissing;
  if (b.hit.atMs) h["ageS"] = ageS(b.hit.atMs);
  if (b.hit.valid) {
    put(h, "maxChargeA", b.hit.maxChargeA);
    put(h, "maxDischargeA", b.hit.maxDischargeA);
    put(h, "chargeCutV", b.hit.chargeCutV);
    put(h, "dischargeCutV", b.hit.dischargeCutV);
    put(h, "modTempMax", b.hit.modTempMax, 0);
    put(h, "modTempMin", b.hit.modTempMin, 0);
    put(h, "cellTempMax", b.hit.cellTempMax, 0);
    put(h, "cellTempMin", b.hit.cellTempMin, 0);
    put(h, "modVMax", b.hit.modVMax);
    put(h, "modVMin", b.hit.modVMin);
    put(h, "cellVMax", b.hit.cellVMax);
    put(h, "cellVMin", b.hit.cellVMin);
  }

  JsonArray pa = doc["packs"].to<JsonArray>();
  for (uint8_t i = 0; i < b.packs; i++) {
    JsonObject p = pa.add<JsonObject>();
    p["sn"] = b.pack[i].sn;
    p["bmsFw"] = b.pack[i].bmsFw;
    p["ratedCap"] = b.pack[i].ratedCap;
    p["dcdcSw"] = b.pack[i].dcdcSw;
  }

  JsonObject c = doc["cloudData"].to<JsonObject>();
  c["valid"] = b.cloud.valid;
  if (b.cloud.atMs) c["ageS"] = ageS(b.cloud.atMs);
  if (b.cloudErr[0]) c["error"] = b.cloudErr;
  if (b.cloud.sn[0]) c["sn"] = b.cloud.sn;
  if (b.cloud.model[0]) c["model"] = b.cloud.model;
  if (b.cloud.valid) {
    put(c, "soc", b.cloud.soc, 0);
    put(c, "soh", b.cloud.soh, 0);
    put(c, "powerW", b.cloud.powerW, 0);
    put(c, "voltV", b.cloud.voltV);
    put(c, "currA", b.cloud.currA);
    put(c, "tempMax", b.cloud.tempMax);
    put(c, "tempMin", b.cloud.tempMin);
    put(c, "mosTemp", b.cloud.mosTemp);
    put(c, "cellVMax", b.cloud.cellVMax, 3);
    put(c, "cellVMin", b.cloud.cellVMin, 3);
    put(c, "cellDiffMv", b.cloud.cellDiffMv, 0);
    put(c, "capKwh", b.cloud.capKwh, 2);
    put(c, "remainKwh", b.cloud.remainKwh, 2);
    put(c, "chargeLimA", b.cloud.chargeLimA);
    put(c, "dischargeLimA", b.cloud.dischargeLimA);
    put(c, "cycles", b.cloud.cycles, 0);
    if (b.cloud.alarm >= 0) c["alarm"] = b.cloud.alarm;
    if (b.cloud.cells) {
      JsonArray cv = c["cells"].to<JsonArray>();
      for (uint8_t i = 0; i < b.cloud.cells; i++) cv.add(serialized(String(b.cloud.cellV[i], 3)));
    }
  }
}

} // namespace BatteryData
