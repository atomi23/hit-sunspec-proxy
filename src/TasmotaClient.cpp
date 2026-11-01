#include "TasmotaClient.h"
#include "LiveData.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

namespace {
AppConfig local;
uint32_t nextPoll = 0;
bool started = false;

void buildUrl(char *url, size_t n, const char *cmnd) {
  if (local.tasmotaUser[0] && local.tasmotaPass[0]) {
    snprintf(url, n, "http://%s:%u/cm?user=%s&password=%s&cmnd=%s",
             local.tasmotaHost, local.tasmotaPort,
             local.tasmotaUser, local.tasmotaPass, cmnd);
  } else {
    snprintf(url, n, "http://%s:%u/cm?cmnd=%s",
             local.tasmotaHost, local.tasmotaPort, cmnd);
  }
}

bool parseEnergy(JsonVariantConst root, String *sensorKeyOut = nullptr) {
  JsonVariantConst energy;
  const char *keyFound = nullptr;

  if (!root["StatusSNS"].isNull() && !root["StatusSNS"]["ENERGY"].isNull()) {
    energy = root["StatusSNS"]["ENERGY"];
    keyFound = "ENERGY";
  } else if (!root["ENERGY"].isNull()) {
    energy = root["ENERGY"];
    keyFound = "ENERGY";
  } else if (!root["StatusSNS"].isNull()) {
    JsonObjectConst sns = root["StatusSNS"].as<JsonObjectConst>();
    for (auto kv : sns) {
      if (kv.key() == "Time") continue;
      auto v = kv.value();
      if (!v["Power"].isNull() || !v["Total"].isNull() || !v["E_in"].isNull() || !v["E_out"].isNull()) {
        energy = v;
        keyFound = kv.key().c_str();
        break;
      }
    }
  }
  if (energy.isNull()) return false;

  if (sensorKeyOut && keyFound) *sensorKeyOut = keyFound;
  strlcpy(gLive.tasmotaSensorKey, keyFound ? keyFound : "", sizeof(gLive.tasmotaSensorKey));

  if (!energy["Power"].isNull()) gLive.tasmotaPowerW = energy["Power"].as<int32_t>();
  else if (!energy["Power_cur"].isNull()) gLive.tasmotaPowerW = energy["Power_cur"].as<int32_t>();

  if (!energy["Total"].isNull()) gLive.tasmotaTotalKwh = energy["Total"].as<double>();
  else if (!energy["E_in"].isNull()) gLive.tasmotaTotalKwh = energy["E_in"].as<double>();
  if (!energy["TotalStart"].isNull() && gLive.tasmotaTotalKwh <= 0) {
    gLive.tasmotaTotalKwh = energy["TotalStart"].as<double>();
  }

  if (!energy["Export"].isNull()) gLive.tasmotaExportKwh = energy["Export"].as<double>();
  else if (!energy["TotalExport"].isNull()) gLive.tasmotaExportKwh = energy["TotalExport"].as<double>();
  else if (!energy["E_out"].isNull()) gLive.tasmotaExportKwh = energy["E_out"].as<double>();
  else if (!energy["Heute_Einsp"].isNull()) gLive.tasmotaExportKwh = energy["Heute_Einsp"].as<double>();

  if (!energy["Meter_Number"].isNull()) {
    strlcpy(gLive.tasmotaMeterId, energy["Meter_Number"].as<const char *>(), sizeof(gLive.tasmotaMeterId));
  }

  return true;
}

bool fetchStatus8(String &bodyOut) {
  HTTPClient http;
  char url[192];
  buildUrl(url, sizeof(url), "Status%208");
  http.setConnectTimeout(2000);
  http.setTimeout(3000);
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  bodyOut = http.getString();
  http.end();
  return true;
}
} // namespace

namespace TasmotaClient {

void begin(const AppConfig &cfg) {
  local = cfg;
  started = true;
  nextPoll = 0;
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
  nextPoll = 0;
}

bool pollNow() {
  if (!local.tasmotaEnabled) {
    gLive.tasmotaOk = false;
    return false;
  }
  String body;
  if (!fetchStatus8(body)) {
    gLive.tasmotaOk = false;
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    gLive.tasmotaOk = false;
    return false;
  }
  if (!parseEnergy(doc.as<JsonVariantConst>())) {
    gLive.tasmotaOk = false;
    return false;
  }
  gLive.tasmotaOk = true;
  gLive.lastTasmotaMs = millis();
  return true;
}

bool probe(String &rawOut, String &summaryOut) {
  rawOut = "";
  summaryOut = "";
  String body;
  if (!fetchStatus8(body)) {
    summaryOut = "HTTP fehlgeschlagen — IP/Netz/Passwort prüfen";
    gLive.tasmotaOk = false;
    return false;
  }
  rawOut = body;
  JsonDocument doc;
  if (deserializeJson(doc, body)) {
    summaryOut = "JSON ungültig";
    gLive.tasmotaOk = false;
    return false;
  }
  String key;
  if (!parseEnergy(doc.as<JsonVariantConst>(), &key)) {
    summaryOut = "Kein Zähler-Block gefunden (Power/E_in/E_out/Total)";
    gLive.tasmotaOk = false;
    return false;
  }
  gLive.tasmotaOk = true;
  gLive.lastTasmotaMs = millis();
  char buf[192];
  snprintf(buf, sizeof(buf),
           "OK · Sensor %s · Power %ld W · Bezug %.3f kWh · Einsp. %.3f kWh%s%s",
           gLive.tasmotaSensorKey,
           long(gLive.tasmotaPowerW),
           gLive.tasmotaTotalKwh,
           gLive.tasmotaExportKwh,
           gLive.tasmotaMeterId[0] ? " · Nr " : "",
           gLive.tasmotaMeterId[0] ? gLive.tasmotaMeterId : "");
  summaryOut = buf;
  return true;
}

void loop() {
  if (!started || !local.tasmotaEnabled) return;
  if (millis() < nextPoll) return;
  nextPoll = millis() + max<uint32_t>(2000, local.tasmotaPollMs);
  pollNow();
}

} // namespace TasmotaClient
