#include "HoyCloud.h"
#include "LiveData.h"
#include "Watchdog.h"
#include "EventLog.h"
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <WiFi.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>

namespace {
AppConfig local;
bool started = false;
uint32_t nextPoll = 0;
char token[196] = "";
uint32_t tokenAt = 0;
uint32_t loginBlockedUntil = 0; // nach Fehlversuch nicht dauernd anmelden, sonst sperrt S-Miles das Konto

// neapi.hoymiles.com: GeoTrust G2 → DigiCert Global Root G2 (Cross-Sign über Global Root CA)
const char HOY_CA[] PROGMEM = R"PEM(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)PEM";

String md5Hex(const char *s) {
  MD5Builder b;
  b.begin();
  b.add(s);
  b.calculate();
  return b.toString();
}

int lastHttpCode = 0;

bool httpsPost(const String &url, const String &body, const char *auth, String &out) {
  NetworkClientSecure cli;
  cli.setCACert(HOY_CA);
  // Der Abruf läuft in der Hauptschleife (auch SunSpec für Consolinno): ohne Grenze wartet der
  // TLS-Handshake bis zu 120 s, dann schlägt der Watchdog zu
  cli.setHandshakeTimeout(10);
  cli.setTimeout(10);
  HTTPClient http;
  if (!http.begin(cli, url)) return false;
  http.setConnectTimeout(5000);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (auth && auth[0]) http.addHeader("Authorization", auth);
  Watchdog::feed();
  Watchdog::stage("Cloud: Verbindung");
  int code = http.POST(body);
  lastHttpCode = code;
  Watchdog::feed();
  Watchdog::stage("Cloud: Antwort");
  out = http.getString();
  http.end();
  Watchdog::stage("Cloud");
  Watchdog::feed();
  return code >= 200 && code < 300;
}

bool login() {
  if (!local.cloudUser[0] || !local.cloudPass[0]) return false;
  if (loginBlockedUntil && int32_t(millis() - loginBlockedUntil) < 0) return false;
  const String host = local.cloudHost[0] ? local.cloudHost : "neapi.hoymiles.com";
  const String passMd5 = md5Hex(local.cloudPass);

  // v3 braucht Nonce aus pre-insp und ein anderes Hash-Verfahren; v0 mit MD5 funktioniert weiter
  const char *paths[] = {
      "/iam/pub/0/auth/login",
  };
  bool answered = false;
  for (const char *path : paths) {
    JsonDocument req;
    req["user_name"] = local.cloudUser;
    req["password"] = passMd5;
    String body;
    serializeJson(req, body);
    String resp;
    const bool ok = httpsPost("https://" + host + path, body, nullptr, resp);
    Serial.printf("Cloud login %s: HTTP %d, %u Bytes\n", path, lastHttpCode, unsigned(resp.length()));
    if (lastHttpCode > 0) answered = true;
    if (!ok) continue;

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
    Serial.printf("Cloud login abgelehnt: %.160s\n", resp.c_str()); // ohne Token, nur Fehlermeldung
    EventLog::add('W', "Cloud", "Login abgelehnt (HTTP %d), nächster Versuch in 5 min", lastHttpCode);
  }
  token[0] = 0;
  // nur sperren, wenn Hoymiles geantwortet und abgelehnt hat; Netzwerkfehler sofort wieder versuchen
  if (answered) {
    loginBlockedUntil = millis() + 5UL * 60UL * 1000UL;
    strlcpy(gLive.status, "Cloud-Login abgelehnt", sizeof(gLive.status));
  }
  return false;
}

// S-Miles liefert Zahlen teils als Text ("8642900")
float num(JsonVariantConst x) {
  if (x.is<float>() || x.is<int>() || x.is<long>() || x.is<double>()) return x.as<float>();
  if (x.is<const char *>()) {
    const char *s = x.as<const char *>();
    char *end = nullptr;
    float f = strtof(s, &end);
    if (end && end != s && *end == '\0') return f;
  }
  return NAN;
}

// Speicheranlagen: Akku/Netz stehen in reflux_station_data, PV/Energie auf oberster Ebene
float pickF(JsonVariantConst v, const char *k0, const char *k1 = nullptr, const char *k2 = nullptr,
            const char *k3 = nullptr) {
  const char *keys[4] = {k0, k1, k2, k3};
  JsonVariantConst scopes[2] = {v, v["reflux_station_data"]};
  for (JsonVariantConst s : scopes) {
    if (s.isNull()) continue;
    for (const char *k : keys) {
      if (!k) continue;
      float f = num(s[k]);
      if (!isnan(f)) return f;
    }
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

  int firstCode = 0; // der zweite Pfad ist nur ein Ausweichweg; für die Meldung zählt der erste
  for (const char *path : paths) {
    String resp;
    const bool ok = httpsPost("https://" + host + path, body, token, resp);
    if (path == paths[0]) firstCode = lastHttpCode;
    static bool dumped = false;
    if (!dumped && ok) {
      dumped = true;
      Serial.printf("Cloud data %s: HTTP %d, %u Bytes\n", path, lastHttpCode, unsigned(resp.length()));
      for (size_t i = 0; i < resp.length(); i += 200) Serial.println(resp.substring(i, i + 200));
    } else if (!ok) {
      Serial.printf("Cloud data %s: HTTP %d\n", path, lastHttpCode);
    }
    if (!ok) continue;
    // Protobuf/binary → kein JSON
    if (resp.length() < 2 || (resp[0] != '{' && resp[0] != '[')) continue;

    JsonDocument doc;
    if (deserializeJson(doc, resp)) continue;
    JsonVariantConst data = doc.as<JsonVariantConst>();
    if (!doc["data"].isNull() && doc["data"].is<JsonObjectConst>()) {
      data = doc["data"].as<JsonVariantConst>();
    }

    float pv = pickF(data, "pv_power", "real_power", "power", "generation_power");
    float bat = pickF(data, "bms_power", "battery_power", "p_bat", "bat_power");
    float grid = pickF(data, "grid_power", "p_grid", "meter_power", "pgrid");
    float soc = pickF(data, "bms_soc", "soc", "battery_soc");
    float ePv = pickF(data, "total_eq", "energy_total", "eq", "pv_energy");
    // Zählerstände in Wh unter reflux_station_data.grid_in_eq / grid_out_eq
    JsonVariantConst reflux = data["reflux_station_data"];
    float eImp = num(reflux["grid_in_eq"]["total_eq"]);
    float eExp = num(reflux["grid_out_eq"]["total_eq"]);

    if (isnan(pv) && isnan(bat) && isnan(soc)) continue;

    if (!isnan(pv)) gLive.pvW = int32_t(lroundf(pv));
    if (!isnan(bat)) gLive.batW = int32_t(lroundf(bat));
    // S-Miles: grid_power negativ = Bezug (load = pv + bms − grid); bei uns ist Bezug positiv
    if (!isnan(grid)) gLive.gridW = -int32_t(lroundf(grid));
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

    if (!gLive.cloudOk) EventLog::add('I', "Cloud", "Daten kommen an");
    gLive.cloudOk = true;
    gLive.lastCloudMs = millis();
    return true;
  }

  if (gLive.cloudOk) {
    if (firstCode > 0) EventLog::add('W', "Cloud", "Datenabruf fehlgeschlagen (HTTP %d)", firstCode);
    else EventLog::add('W', "Cloud", "Datenabruf fehlgeschlagen (keine Verbindung, Fehler %d)", firstCode);
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
  loginBlockedUntil = 0;
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
  // erst wenn das Netz ein paar Sekunden steht, sonst scheitert schon die Namensauflösung
  static uint32_t netUpAt = 0;
  if (!gLive.ethOk && WiFi.status() != WL_CONNECTED) {
    netUpAt = 0;
    return;
  }
  if (!netUpAt) netUpAt = millis() | 1;
  if (millis() - netUpAt < 5000) return;
  if (int32_t(millis() - nextPoll) < 0) return;
  // die TLS-Verbindung braucht rund 40 KB internen RAM; sind gerade HTTPS-Sitzungen offen, kurz warten
  if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < 70000) {
    nextPoll = millis() + 3000;
    return;
  }
  nextPoll = millis() + max<uint32_t>(15000, local.cloudPollMs);
  pollNow();
}

} // namespace HoyCloud
