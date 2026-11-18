// Dyness Open API v1 (Akku-Cloud). Ablauf und Punktnummern nach der Home-Assistant-Integration
// github.com/shopf/dyness_battery (MIT), Signatur nach deren tools/dyness_test.py.
#include "BatteryCloud.h"
#include "BatteryLogic.h"
#include "HoyCloud.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <mbedtls/md.h>
#include <mbedtls/md5.h>
#include <mbedtls/base64.h>
#include <time.h>

namespace {

String b64(const uint8_t *p, size_t n) {
  unsigned char out[64];
  size_t olen = 0;
  if (mbedtls_base64_encode(out, sizeof(out), &olen, p, n) != 0) return String();
  return String(reinterpret_cast<const char *>(out), olen);
}

String contentMd5(const String &body) {
  uint8_t d[16];
  mbedtls_md5(reinterpret_cast<const unsigned char *>(body.c_str()), body.length(), d);
  return b64(d, sizeof(d));
}

String hmacSha1B64(const char *secret, const String &msg) {
  uint8_t d[20];
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  mbedtls_md_hmac(info, reinterpret_cast<const unsigned char *>(secret), strlen(secret),
                  reinterpret_cast<const unsigned char *>(msg.c_str()), msg.length(), d);
  return b64(d, sizeof(d));
}

String sign(const char *secret, const String &body, const char *date, const char *path, String &md5Out) {
  md5Out = contentMd5(body);
  return hmacSha1B64(secret, dynessStringToSign("POST", md5Out.c_str(), date, path));
}

String httpDate() {
  const time_t now = time(nullptr);
  tm g;
  gmtime_r(&now, &g);
  char buf[40];
  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &g);
  return buf;
}

float num(JsonVariantConst v) {
  if (v.isNull()) return NAN;
  if (v.is<float>()) return v.as<float>();
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    if (!s || !*s) return NAN;
    char *end = nullptr;
    const float f = strtof(s, &end);
    return end == s ? NAN : f;
  }
  return NAN;
}

bool codeOk(JsonVariantConst code) {
  if (code.is<int>()) return code.as<int>() == 0 || code.as<int>() == 200;
  const char *s = code.as<const char *>();
  return s && (!strcmp(s, "0") || !strcmp(s, "200"));
}

bool rateLimited(int http, JsonDocument &doc) {
  if (http == 429) return true;
  const char *c = doc["code"].as<const char *>();
  if ((c && !strcmp(c, "429")) || doc["code"].as<int>() == 429) return true;
  const char *sc = doc["sourceCode"].as<const char *>();
  return sc && !strcasecmp(sc, "TOO_MANY_REQUESTS");
}

class Dyness : public BatteryCloudDriver {
public:
  CloudResult poll(const AppConfig &cfg, BatteryCloudData &io, String &err) override {
    if (!sn_[0]) {
      if (cfg.batCloudSn[0]) {
        strlcpy(sn_, cfg.batCloudSn, sizeof(sn_));
      } else {
        return discover(cfg, io, err);
      }
    }
    strlcpy(io.sn, sn_, sizeof(io.sn));
    if (!bound_) return bind(cfg, err);
    if (!stationAt_ || millis() - stationAt_ > 24UL * 3600UL * 1000UL) return station(cfg, io, err);
    const bool rt = !powerNext_;
    powerNext_ = !powerNext_;
    CloudResult r = rt ? realtime(cfg, io, err) : power(cfg, io, err);
    if (r == CloudResult::Data && firstRound_) {
      if (!rt) firstRound_ = false; // nach Echtzeit und Leistung ist die erste Runde vollständig
      else r = CloudResult::DataMore;
    }
    return r;
  }

  void reset() override {
    sn_[0] = collector_[0] = 0;
    bound_ = false;
    stationAt_ = 0;
    powerNext_ = false;
    firstRound_ = true;
  }

private:
  char sn_[40] = "";
  char collector_[40] = "";
  bool bound_ = false;
  uint32_t stationAt_ = 0;
  bool powerNext_ = false;
  bool firstRound_ = true;

  // Schlüssel in alphabetischer Reihenfolge, wie die Referenz (sort_keys)
  String body(bool withCollector, bool paged) {
    JsonDocument d;
    if (withCollector && collector_[0]) d["collectorSn"] = collector_;
    d["deviceSn"] = sn_;
    if (paged) {
      d["pageNo"] = 1;
      d["pageSize"] = 1;
    }
    String s;
    serializeJson(d, s);
    return s;
  }

  // Eine Anfrage; res bekommt die gefilterte Antwort. Rückgabe wie poll.
  CloudResult post(const AppConfig &cfg, const char *path, const String &payload, JsonDocument &res,
                   JsonDocument &filter, String &err) {
    NetworkClientSecure cli;
    cli.setCACert(CLOUD_CA_DIGICERT);
    cli.setHandshakeTimeout(10);
    HTTPClient http;
    http.useHTTP10(true); // keine Chunks, damit die Antwort direkt aus dem Datenstrom gelesen werden kann
    const String url = String("https://") + cfg.batCloudHost + "/openapi/ems-device" + path;
    if (!http.begin(cli, url)) {
      err = "URL ungültig";
      return CloudResult::Error;
    }
    http.setConnectTimeout(5000);
    http.setTimeout(10000);
    const String date = httpDate();
    String md5;
    const String sig = sign(cfg.batCloudSecret, payload, date.c_str(), path, md5);
    http.addHeader("Content-Type", "application/json;charset=UTF-8");
    http.addHeader("Content-MD5", md5);
    http.addHeader("Date", date);
    http.addHeader("Authorization", String("API ") + cfg.batCloudAppId + ":" + sig);
    const int code = http.POST(payload);
    if (code <= 0) {
      err = String("keine Verbindung (") + code + ")";
      http.end();
      return CloudResult::Error;
    }
    const DeserializationError de =
        deserializeJson(res, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (rateLimited(code, res)) return CloudResult::RateLimited;
    if (code == 401 || code == 403) {
      err = "Anmeldung abgelehnt (App-ID, Secret oder Uhrzeit prüfen)";
      return CloudResult::Error;
    }
    if (de) {
      err = String("HTTP ") + code + ", Antwort unlesbar";
      return CloudResult::Error;
    }
    if (!codeOk(res["code"])) {
      const char *info = res["info"].as<const char *>();
      err = String("Fehler ") + (res["code"].as<const char *>() ? res["code"].as<const char *>() : String(code).c_str()) +
            (info ? String(": ") + info : String());
      return CloudResult::Error;
    }
    return CloudResult::Data;
  }

  CloudResult discover(const AppConfig &cfg, BatteryCloudData &io, String &err) {
    JsonDocument filter, res;
    filter["code"] = true;
    filter["info"] = true;
    filter["sourceCode"] = true;
    JsonObject f = filter["data"]["list"][0].to<JsonObject>();
    f["deviceSn"] = true;
    f["collectorSn"] = true;
    f["deviceModelName"] = true;
    const CloudResult r = post(cfg, "/v1/device/storage/list", "{}", res, filter, err);
    if (r != CloudResult::Data) return r;
    JsonArrayConst list = res["data"]["list"].as<JsonArrayConst>();
    JsonVariantConst pick;
    for (JsonVariantConst d : list) {
      const char *s = d["deviceSn"] | "";
      const size_t n = strlen(s);
      if (n > 4 && (!strcmp(s + n - 4, "-BMS") || !strcmp(s + n - 4, "-BDU"))) {
        pick = d;
        break;
      }
    }
    if (pick.isNull() && list.size()) pick = list[0];
    if (pick.isNull()) {
      err = "kein Gerät im Dyness-Konto";
      return CloudResult::Error;
    }
    strlcpy(sn_, pick["deviceSn"] | "", sizeof(sn_));
    strlcpy(collector_, pick["collectorSn"] | "", sizeof(collector_));
    strlcpy(io.model, pick["deviceModelName"] | "", sizeof(io.model));
    strlcpy(io.sn, sn_, sizeof(io.sn));
    return CloudResult::Busy;
  }

  CloudResult bind(const AppConfig &cfg, String &err) {
    JsonDocument filter, res;
    filter["code"] = true;
    filter["info"] = true;
    filter["sourceCode"] = true;
    CloudResult r = post(cfg, "/v1/device/bindSn", body(true, false), res, filter, err);
    // 500 heißt bei Dyness meist "schon gebunden"
    const char *c = res["code"].as<const char *>();
    if (r == CloudResult::Error && ((c && !strcmp(c, "500")) || res["code"].as<int>() == 500)) r = CloudResult::Data;
    if (r != CloudResult::Data) return r;
    err = "";
    bound_ = true;
    return CloudResult::Busy;
  }

  CloudResult station(const AppConfig &cfg, BatteryCloudData &io, String &err) {
    JsonDocument filter, res;
    filter["code"] = true;
    filter["info"] = true;
    filter["sourceCode"] = true;
    filter["data"]["batteryCapacity"] = true;
    const CloudResult r = post(cfg, "/v1/station/info", body(false, false), res, filter, err);
    if (r == CloudResult::RateLimited) return r;
    stationAt_ = millis() | 1; // auch bei Fehler nicht gleich wiederholen; Kapazität ist nur Zusatz
    if (r == CloudResult::Data) {
      const float cap = num(res["data"]["batteryCapacity"]);
      if (!isnan(cap) && cap > 0) io.capKwh = cap;
    }
    err = "";
    return CloudResult::Busy;
  }

  CloudResult realtime(const AppConfig &cfg, BatteryCloudData &io, String &err) {
    JsonDocument filter, res;
    filter["code"] = true;
    filter["info"] = true;
    filter["sourceCode"] = true;
    filter["data"][0]["pointId"] = true;
    filter["data"][0]["pointValue"] = true;
    const CloudResult r = post(cfg, "/v1/device/realTime/data", body(true, false), res, filter, err);
    if (r != CloudResult::Data) return r;
#ifdef BOARD_HAS_PSRAM
    const bool keepCells = true;
#else
    const bool keepCells = false;
#endif
    dynessParsePoints(res["data"].as<JsonArrayConst>(), io, keepCells);
    finish(io);
    return CloudResult::Data;
  }

  CloudResult power(const AppConfig &cfg, BatteryCloudData &io, String &err) {
    JsonDocument filter, res;
    filter["code"] = true;
    filter["info"] = true;
    filter["sourceCode"] = true;
    JsonObject e = filter["data"][0].to<JsonObject>();
    e["soc"] = true;
    e["realTimePower"] = true;
    e["realTimeCurrent"] = true;
    const CloudResult r = post(cfg, "/v1/device/getLastPowerDataBySn", body(true, true), res, filter, err);
    if (r != CloudResult::Data) return r;
    JsonVariantConst last;
    for (JsonVariantConst d : res["data"].as<JsonArrayConst>())
      if (!d["soc"].isNull()) last = d;
    if (!last.isNull()) {
      const float soc = num(last["soc"]);
      if (!isnan(soc) && soc >= 0 && soc <= 100) io.soc = soc;
      const float p = num(last["realTimePower"]);
      if (!isnan(p)) io.powerW = p;
      const float a = num(last["realTimeCurrent"]);
      if (!isnan(a)) io.currA = a;
    }
    finish(io);
    return CloudResult::Data;
  }

  void finish(BatteryCloudData &io) {
    io.valid = true;
    io.atMs = millis();
    strlcpy(io.sn, sn_, sizeof(io.sn));
    if (!isnan(io.capKwh) && !isnan(io.soc)) {
      const float soh = (!isnan(io.soh) && io.soh > 0 && io.soh <= 100) ? io.soh / 100.0f : 1.0f;
      io.remainKwh = io.capKwh * soh * io.soc / 100.0f;
    }
  }
};

Dyness gDyness;
} // namespace

BatteryCloudDriver &dynessDriver() { return gDyness; }

// Testvektor, berechnet mit dem Verfahren aus tools/dyness_test.py (siehe tools/battery_check.py --sign-selftest):
// Secret "test-secret", Body {"deviceSn":"X"}, Date "Tue, 18 Nov 2026 10:00:00 GMT", Pfad /v1/device/realTime/data
//   Content-MD5 K18hg9F3/G050B4mkxEGfQ==   Signatur +U7BkOuEwKBtyTqqUQB0kBj8Pig=
bool dynessSignSelftest() {
  String md5;
  const String sig = sign("test-secret", "{\"deviceSn\":\"X\"}", "Tue, 18 Nov 2026 10:00:00 GMT",
                          "/v1/device/realTime/data", md5);
  return md5 == "K18hg9F3/G050B4mkxEGfQ==" && sig == "+U7BkOuEwKBtyTqqUQB0kBj8Pig=";
}
