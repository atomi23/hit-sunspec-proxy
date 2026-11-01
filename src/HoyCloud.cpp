#include "HoyCloud.h"
#include "LiveData.h"
#include "Watchdog.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>

namespace {
AppConfig local;
bool started = false;
uint32_t nextPoll = 0;
char token[196] = "";
uint32_t tokenAt = 0;

String md5Hex(const char *s) {
  MD5Builder b;
  b.begin();
  b.add(s);
  b.calculate();
  return b.toString();
}

bool httpsPost(const String &url, const String &body, const char *auth, String &out) {
  NetworkClientSecure cli;
  cli.setInsecure();
  HTTPClient http;
  if (!http.begin(cli, url)) return false;
  http.setTimeout(12000);
  http.addHeader("Content-Type", "application/json");
  if (auth && auth[0]) http.addHeader("Authorization", auth);
  Watchdog::feed();
  int code = http.POST(body);
  Watchdog::feed();
  out = http.getString();
  http.end();
  Watchdog::feed();
  return code >= 200 && code < 300;
}

bool login() {
  if (!local.cloudUser[0] || !local.cloudPass[0]) return false;
  const String host = local.cloudHost[0] ? local.cloudHost : "neapi.hoymiles.com";
  const String passMd5 = md5Hex(local.cloudPass);

  // v0 + v3 Login versuchen
  const char *paths[] = {
      "/iam/pub/0/auth/login",
      "/iam/pub/3/auth/login",
  };
  for (const char *path : paths) {
    JsonDocument req;
    req["user_name"] = local.cloudUser;
    req["password"] = passMd5;
    String body;
    serializeJson(req, body);
    String resp;
    if (!httpsPost("https://" + host + path, body, nullptr, resp)) continue;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) continue;
    const char *tok = nullptr;
    if (doc["data"]["token"].is<const char *>()) tok = doc["data"]["token"];
    else if (doc["token"].is<const char *>()) tok = doc["token"];
    else if (doc["data"]["access_token"].is<const char *>()) tok = doc["data"]["access_token"];
    if (tok && tok[0]) {
      strlcpy(token, tok, sizeof(token));
      tokenAt = millis();
      return true;
    }
  }
  token[0] = 0;
  return false;
}

float pickF(JsonVariantConst v, const char *k0, const char *k1 = nullptr, const char *k2 = nullptr,
            const char *k3 = nullptr) {
  const char *keys[4] = {k0, k1, k2, k3};
  for (int i = 0; i < 4; i++) {
    if (!keys[i]) continue;
    if (v[keys[i]].is<float>() || v[keys[i]].is<int>() || v[keys[i]].is<long>() ||
        v[keys[i]].is<double>())
      return v[keys[i]].as<float>();
  }
  return NAN;
}

bool fetchRealtime() {
  if (!token[0] || (millis() - tokenAt > 50UL * 60UL * 1000UL)) {
    if (!login()) {
      gLive.cloudOk = false;
      return false;
    }
  }
  if (!local.cloudStationId[0]) {
    gLive.cloudOk = false;
    return false;
  }

  const String host = local.cloudHost[0] ? local.cloudHost : "neapi.hoymiles.com";
  long sid = atol(local.cloudStationId);
  JsonDocument req;
  req["sid"] = sid;
  req["station_id"] = sid;
  String body;
  serializeJson(req, body);

  const char *paths[] = {
      "/pvm-data/api/0/station/data/count_station_real_data",
      "/pvm-data/api/0/data/count_station_real_data",
  };

  for (const char *path : paths) {
    String resp;
    if (!httpsPost("https://" + host + path, body, token, resp)) continue;
    // Protobuf/binary → kein JSON
    if (resp.length() < 2 || (resp[0] != '{' && resp[0] != '[')) continue;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) continue;
    JsonVariantConst data = doc.as<JsonVariantConst>();
    if (!doc["data"].isNull() && doc["data"].is<JsonObjectConst>()) {
      data = doc["data"].as<JsonVariantConst>();
    }

    float pv = pickF(data, "real_power", "power", "pv_power", "generation_power");
    float bat = pickF(data, "battery_power", "bms_power", "p_bat", "bat_power");
    float grid = pickF(data, "grid_power", "p_grid", "meter_power", "pgrid");
    float soc = pickF(data, "soc", "battery_soc", "capacitor", "bms_soc");
    float ePv = pickF(data, "total_eq", "energy_total", "eq", "pv_energy");
    float eImp = pickF(data, "grid_in", "import_eq", "energy_imp");
    float eExp = pickF(data, "grid_out", "export_eq", "energy_exp");

    if (isnan(pv) && isnan(bat) && isnan(soc)) continue;

    if (!isnan(pv)) gLive.pvW = int32_t(lroundf(pv));
    if (!isnan(bat)) gLive.batW = int32_t(lroundf(bat));
    if (!isnan(grid)) gLive.gridW = int32_t(lroundf(grid));
    if (!isnan(soc)) {
      uint16_t s = uint16_t(lroundf(soc));
      if (s > 100 && s <= 1000) s = s / 10; // manchmal 0.1%
      if (s > 100) s = 100;
      gLive.soc = s;
    }
    if (!isnan(ePv)) {
      // Hoymiles total_eq oft Wh
      gLive.energyPvKwh = (ePv > 10000.0f) ? (ePv / 1000.0) : double(ePv);
    }
    if (!isnan(eImp)) gLive.energyImpKwh = (eImp > 10000.0f) ? (eImp / 1000.0) : double(eImp);
    if (!isnan(eExp)) gLive.energyExpKwh = (eExp > 10000.0f) ? (eExp / 1000.0) : double(eExp);

    gLive.cloudOk = true;
    gLive.lastCloudMs = millis();
    return true;
  }

  gLive.cloudOk = false;
  return false;
}
} // namespace

namespace HoyCloud {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  nextPoll = 0;
  token[0] = 0;
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
  nextPoll = 0;
  token[0] = 0;
  gLive.cloudOk = false;
}

bool pollNow() {
  if (!local.cloudEnabled) {
    gLive.cloudOk = false;
    return false;
  }
  return fetchRealtime();
}

bool fresh(uint32_t maxAgeMs) {
  return gLive.cloudOk && gLive.lastCloudMs && (millis() - gLive.lastCloudMs) <= maxAgeMs;
}

void loop() {
  if (!started || !local.cloudEnabled) {
    gLive.cloudOk = false;
    return;
  }
  // Nur pollen wenn HIT nicht liefert (DataBroker entscheidet Anzeige)
  if (gLive.hitOk) return;
  if (millis() < nextPoll) return;
  nextPoll = millis() + max<uint32_t>(15000, local.cloudPollMs);
  pollNow();
}

} // namespace HoyCloud
