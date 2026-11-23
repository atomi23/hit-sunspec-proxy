#include "WebConfig.h"
#include "AppConfig.h"
#include "LiveData.h"
#include "HitClient.h"
#include "SunSpecServer.h"
#include "StatusDisplay.h"
#include "TasmotaClient.h"
#include "MqttHa.h"
#include "HitControl.h"
#include "DebugLog.h"
#include "EventLog.h"
#include "HttpsServer.h"
#include "BatteryData.h"
#include "OtaUpdate.h"
#include "HoyCloud.h"
#include "KnxIp.h"
#include "BleLed.h"
#include "SysInfo.h"
#include "KnxPoints.h"
#include "DataSource.h"
#include "UiFonts.h"
#include "Watchdog.h"
#include "History.h"
#include "HitBus.h"
#include "NetBoot.h"
#include "ModbusGateway.h"
#include "DataBroker.h"
#include "WebAssets.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <memory>
#include <new>

// Board für Status und Funktionsauswahl (Display gibt es nur auf dem T-Connect-Pro)
#if defined(BOARD_T_CONNECT_PRO)
#define BOARD_NAME "T-Connect-Pro"
#define BOARD_HAS_DISPLAY true
#elif defined(BOARD_WT32_ETH01)
#define BOARD_NAME "WT32-ETH01"
#define BOARD_HAS_DISPLAY false
#elif defined(BOARD_P4_POE_ETH)
#define BOARD_NAME "ESP32-P4-POE-ETH"
#define BOARD_HAS_DISPLAY false
#elif defined(BOARD_T_POE_PRO)
#define BOARD_NAME "T-POE-Pro"
#define BOARD_HAS_DISPLAY false
#else
#define BOARD_NAME "ESP32"
#define BOARD_HAS_DISPLAY false
#endif

namespace {
AsyncWebServer server(80);

size_t logLine(char *line, size_t size, const SysEvt &e, bool) {
  return snprintf(line, size, "ms=%lu ts=%lu [%c] %s: %s\n",
                  (unsigned long)e.ms, (unsigned long)e.epoch, e.level, e.src, e.msg);
}

size_t logLine(char *line, size_t size, const DebugEvt &e, bool en) {
  return snprintf(line, size, en ? "ms=%lu FC=%u Addr=%u Value=%u %s\n" : "ms=%lu FC=%u Addr=%u Wert=%u %s\n",
                  (unsigned long)e.ms, e.fc, e.addr, e.value, e.note);
}

template <typename Event, size_t Capacity>
void downloadLog(AsyncWebServerRequest *req, const char *type,
                 size_t (*copyTo)(Event *, size_t)) {
  const bool en = ConfigStore::uiEnglish();
  struct Snapshot {
    Event events[Capacity];
    size_t count;
  };
  auto snapshot = std::shared_ptr<Snapshot>(new (std::nothrow) Snapshot);
  if (!snapshot) {
    req->send(503, "text/plain; charset=utf-8",
              en ? "Not enough memory for the download.\n" : "Nicht genug Speicher für den Download.\n");
    return;
  }
  snapshot->count = copyTo(snapshot->events, Capacity);
  auto *response = req->beginChunkedResponse("text/plain; charset=utf-8",
      [snapshot, en](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t written = 0;
        char line[256];
        // Der Byte-Index macht auch kleine Chunks und wiederholte Aufrufe unabhängig.
        for (size_t i = 0; i <= snapshot->count && written < maxLen; i++) {
          size_t len;
          if (i == 0) {
            strlcpy(line, en ? "HIT-Proxy log (oldest entries first; ms since startup, ts Unix time or 0)\n"
                             : "HIT-Proxy Protokoll (älteste Einträge zuerst; ms seit Start, ts Unixzeit oder 0)\n", sizeof(line));
            len = strlen(line);
          } else {
            len = logLine(line, sizeof(line), snapshot->events[i - 1], en);
            if (len >= sizeof(line)) len = sizeof(line) - 1;
          }
          if (index >= len) {
            index -= len;
            continue;
          }
          const size_t n = (len - index < maxLen - written) ? len - index : maxLen - written;
          memcpy(buffer + written, line + index, n);
          written += n;
          index = 0;
        }
        return written;
      });
  String hostname = WiFi.getHostname() ? WiFi.getHostname() : "hit-proxy";
  for (size_t i = 0; i < hostname.length(); i++) {
    const char c = hostname[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) hostname.setCharAt(i, '_');
  }
  response->addHeader("Content-Disposition", "attachment; filename=\"hit-proxy-" + String(type) + "-" + hostname + ".txt\"");
  response->addHeader("Cache-Control", "no-store");
  req->send(response);
}

// Basic-Auth schickt der Browser auch bei Anfragen fremder Seiten mit (CSRF). Ändernde Anfragen
// deshalb nur von der eigenen Oberfläche annehmen. Werkzeuge ohne Origin/Sec-Fetch-Site (curl,
// Python) bleiben erlaubt. Über den HTTPS-Proxy (127.0.0.1) kommt der Host als X-Forwarded-Host.
bool crossSite(AsyncWebServerRequest *req) {
  const auto m = req->method();
  if (m == HTTP_GET || m == HTTP_HEAD || m == HTTP_OPTIONS) return false;
  if (req->hasHeader("Sec-Fetch-Site")) {
    const String &s = req->header("Sec-Fetch-Site");
    return !(s.equalsIgnoreCase("same-origin") || s.equalsIgnoreCase("none"));
  }
  if (!req->hasHeader("Origin")) return false;
  String origin = req->header("Origin");
  const int p = origin.indexOf("://");
  if (p >= 0) origin = origin.substring(p + 3);
  String host = req->host();
  const bool local = req->client() && req->client()->remoteIP() == IPAddress(127, 0, 0, 1);
  if (local && req->hasHeader("X-Forwarded-Host")) host = req->header("X-Forwarded-Host");
  return !origin.equalsIgnoreCase(host);
}

bool requireAuth(AsyncWebServerRequest *req) {
  if (crossSite(req)) {
    req->send(403, "application/json", ConfigStore::uiText(
        "{\"ok\":false,\"error\":\"Anfrage von einer anderen Website abgelehnt.\"}",
        "{\"ok\":false,\"error\":\"Request from another website rejected.\"}"));
    return false;
  }
  String cookies = req->header("Cookie");
  while (cookies.length()) {
    const int end = cookies.indexOf(';');
    String cookie = end < 0 ? cookies : cookies.substring(0, end);
    cookie.trim();
    if (cookie == "hp_lo=1") {
      const bool navigate = req->hasHeader("Sec-Fetch-Mode")
          ? req->header("Sec-Fetch-Mode").equalsIgnoreCase("navigate")
          : req->header("Accept").indexOf("text/html") >= 0;
      auto *r = req->beginResponse(401, "text/plain; charset=utf-8", ConfigStore::uiText(
          "Abgemeldet. Seite neu laden zum Anmelden.", "Signed out. Reload the page to sign in."));
      r->addHeader("Cache-Control", "no-store");
      if (navigate) {
        r->addHeader("WWW-Authenticate", "Basic realm=\"HIT-Proxy\"");
        r->addHeader("Set-Cookie", "hp_lo=; Path=/; Max-Age=0; SameSite=Strict; HttpOnly");
      }
      req->send(r);
      return false;
    }
    if (end < 0) break;
    cookies = cookies.substring(end + 1);
  }
  auto &c = ConfigStore::get();
  const char *user = c.webUser[0] ? c.webUser : "admin";
  if (c.displayCode[0] && req->authenticate(user, c.displayCode)) return true;
  if (c.webPass[0] && req->authenticate(user, c.webPass)) return true;
  if (c.wifiPass[0] && req->authenticate(user, c.wifiPass)) return true;

  Serial.printf("Auth fail user=%s\n", user);
  auto *r = req->beginResponse(401, "text/plain; charset=utf-8", ConfigStore::uiText(
      "Abgemeldet oder nicht angemeldet. Seite neu laden zum Anmelden.",
      "Signed out or not signed in. Reload the page to sign in."));
  r->addHeader("WWW-Authenticate", "Basic realm=\"HIT-Proxy\"");
  r->addHeader("Cache-Control", "no-store");
  req->send(r);
  return false;
}

// Der Body kommt in mehreren TCP-Stücken; erst das letzte liefert den ganzen Text.
// _tempObject gibt AsyncWebServer beim Ende des Requests selbst frei.
const char *fullBody(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total,
                     size_t maxBody = 16384) {
  if (total > maxBody) {
    if (index == 0) req->send(413, "application/json", ConfigStore::uiText(
        "{\"ok\":false,\"error\":\"Die Anfrage ist zu groß.\"}",
        "{\"ok\":false,\"error\":\"The request is too large.\"}"));
    return nullptr;
  }
  if (index == 0) {
    // Anmeldung prüfen, bevor Speicher reserviert wird (Header sind schon da)
    if (!requireAuth(req)) return nullptr;
    req->_tempObject = calloc(total + 1, 1);
    if (!req->_tempObject) {
      req->send(500, "application/json", ConfigStore::uiText(
          "{\"ok\":false,\"error\":\"Nicht genug Arbeitsspeicher.\"}",
          "{\"ok\":false,\"error\":\"Not enough memory.\"}"));
      return nullptr;
    }
  }
  if (!req->_tempObject || index + len > total) return nullptr;
  memcpy(static_cast<char *>(req->_tempObject) + index, data, len);
  if (index + len < total) return nullptr;
  return static_cast<const char *>(req->_tempObject);
}

// Netzwerk neu starten oder den HIT abfragen dauert Sekunden und kann die Verbindung kappen, über die
// gespeichert wurde. Deshalb erst antworten, dann in der Hauptschleife übernehmen.
volatile uint8_t applyPending = 0; // 1 = übernehmen, 2 = übernehmen und HIT sofort abfragen
volatile uint32_t applyAt = 0;
// Langsame Arbeit nie im async_tcp-Task (SPI/HTTP/MQTT/HIT)
volatile uint8_t workFlags = 0;
constexpr uint8_t W_POLL = 1, W_CTRL = 2, W_NEWCODE = 4;
void addWork(uint8_t w) { __atomic_fetch_or(&workFlags, w, __ATOMIC_SEQ_CST); }
volatile bool ctrlDone = false, ctrlOk = false;
char ctrlErr[96] = "";
AppConfig probeReq;
SemaphoreHandle_t probeLock = nullptr;
enum class ProbeState { Idle, Queued, Running, Done };
ProbeState probeState = ProbeState::Idle;
String probeResult;

void scheduleApply(bool pollHit) {
  applyAt = millis() + 300;
  applyPending = pollHit ? 2 : (applyPending ? applyPending : 1);
}

void runPendingApply() {
  if (!applyPending || int32_t(millis() - applyAt) < 0) return;
  const bool poll = applyPending == 2;
  applyPending = 0;
  auto &c = ConfigStore::get();
  NetBoot::applyConfig(c);
  HitBus::applyConfig(c);
  HitClient::applyConfig(c);
  ModbusGateway::applyConfig(c);
  HitControl::applyConfig(c);
  SunSpecServer::applyConfig(c);
  StatusDisplay::applyConfig(c);
  TasmotaClient::applyConfig(c);
  MqttHa::applyConfig(c);
  HoyCloud::applyConfig(c);
  BleLed::applyConfig(c);
  KnxIp::applyConfig(c);
  HttpsServer::applyConfig(c);
  if (poll) HitClient::pollNow();
  SunSpecServer::rebuildMap();
}

void runDeferredWork() {
  // Erst die gerade gespeicherten Einstellungen übernehmen, sonst pollt/schreibt man mit den alten
  if (applyPending) return;
  // Web-Task setzt Bits auf dem anderen Kern: atomar abholen, sonst geht ein Auftrag verloren
  const uint8_t f = __atomic_exchange_n(&workFlags, uint8_t(0), __ATOMIC_SEQ_CST);
  if (!f) return;
  if (f & W_NEWCODE) {
    auto &c = ConfigStore::get();
    NetBoot::applyConfig(c);
    StatusDisplay::applyConfig(c);
  }
  if (f & W_POLL) {
    HitClient::pollNow();
    DataBroker::reconcile();
    TasmotaClient::pollNow();
    SunSpecServer::rebuildMap();
    MqttHa::publishNow();
  }
  if (f & W_CTRL) {
    ctrlOk = HitControl::applyAll();
    strlcpy(ctrlErr, HitControl::lastError().c_str(), sizeof(ctrlErr));
    ctrlDone = true;
  }
}

void runProbe() {
  if (applyPending || !probeLock || !xSemaphoreTake(probeLock, 0)) return;
  const auto state = probeState;
  if (state == ProbeState::Queued) probeState = ProbeState::Running;
  xSemaphoreGive(probeLock);
  if (state == ProbeState::Queued) {
    TasmotaClient::requestProbe(probeReq);
  } else if (state == ProbeState::Running) {
    String raw, summary;
    bool ok;
    if (!TasmotaClient::takeProbeResult(raw, summary, ok)) return;
    auto &c = ConfigStore::get();
    if (ok) {
      c.tasmotaEnabled = true;
      strlcpy(c.tasmotaHost, probeReq.tasmotaHost, sizeof(c.tasmotaHost));
      c.tasmotaPort = probeReq.tasmotaPort;
      strlcpy(c.tasmotaUser, probeReq.tasmotaUser, sizeof(c.tasmotaUser));
      if (probeReq.tasmotaPass[0])
        strlcpy(c.tasmotaPass, probeReq.tasmotaPass, sizeof(c.tasmotaPass));
      ConfigStore::save();
      TasmotaClient::applyConfig(c);
      SunSpecServer::rebuildMap();
    }
    JsonDocument out;
    out["pending"] = false;
    out["ok"] = ok;
    out["summary"] = summary;
    if (raw.length() > 2047) raw.remove(2047);
    out["raw"] = raw;
    out["powerW"] = gLive.tasmotaPowerW;
    out["e_in"] = gLive.tasmotaTotalKwh;
    out["e_out"] = gLive.tasmotaExportKwh;
    out["sensor"] = gLive.tasmotaSensorKey;
    String response;
    serializeJson(out, response);
    xSemaphoreTake(probeLock, portMAX_DELAY);
    probeResult = std::move(response);
    probeState = ProbeState::Done;
    xSemaphoreGive(probeLock);
  }
}

void jsonEscapeAppend(String &out, const char *s) {
  for (; s && *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if (uint8_t(*s) >= 0x20) out += *s;
  }
}

// Leere Formularfelder kommen als 0: dann den alten Wert behalten
uint16_t port(JsonVariantConst v, uint16_t old) {
  const long p = v.as<long>();
  return (p >= 1 && p <= 65535) ? uint16_t(p) : old;
}
uint8_t unitId(JsonVariantConst v, uint8_t old) {
  const long u = v.as<long>();
  return (u >= 1 && u <= 247) ? uint8_t(u) : old;
}

// Boards ohne Display haben keinen ablesbaren Code: Zugang nur mit dem eigenen Passwort, das auch für das
// Geräte-WLAN gilt. Geräte, die vor v0.5.13 eingerichtet wurden, behalten ihren Code.
bool ownPassOnly(const AppConfig &c) { return !BOARD_HAS_DISPLAY && !c.displayCode[0]; }

// Ohne eigenes Passwort gilt die Einrichtung nicht als fertig; danach ist das Standard-Passwort überall ungültig
void enforceAccess(AppConfig &c) {
  if (!ownPassOnly(c)) return;
  if (strlen(c.webPass) < MIN_PASS_LEN) c.setupComplete = false;
  else if (c.setupComplete) strlcpy(c.wifiPass, c.webPass, sizeof(c.wifiPass));
}

void applyJson(JsonObjectConst o, AppConfig &c) {
  if (!o["useEthernet"].isNull()) c.useEthernet = o["useEthernet"];
  if (!o["wifiEnabled"].isNull()) c.wifiEnabled = o["wifiEnabled"];
  if (!o["wifiApMode"].isNull()) c.wifiApMode = o["wifiApMode"];
  if (!o["setupComplete"].isNull()) c.setupComplete = o["setupComplete"];
  // leerer Name würde das Geräte-WLAN abschalten (softAP lehnt ihn ab)
  if (o["wifiSsid"].is<const char *>() && o["wifiSsid"].as<const char *>()[0])
    strlcpy(c.wifiSsid, o["wifiSsid"], sizeof(c.wifiSsid));
  if (o["staSsid"].is<const char *>()) strlcpy(c.staSsid, o["staSsid"], sizeof(c.staSsid));
  // Passwort wird nie ausgeliefert; leeres Feld heißt unverändert
  if (o["staPass"].is<const char *>() && o["staPass"].as<const char *>()[0])
    strlcpy(c.staPass, o["staPass"], sizeof(c.staPass));
  if (o["staSsid"].is<const char *>() && !c.staSsid[0]) c.staPass[0] = '\0';
  // wifiPass / displayCode: random — nicht aus Form überschreiben außer explizit
  if (o["webUser"].is<const char *>()) strlcpy(c.webUser, o["webUser"], sizeof(c.webUser));
  // Passwörter nie ausliefern; leeres Feld = unverändert
  if (o["webPass"].is<const char *>() && strlen(o["webPass"].as<const char *>()) >= (ownPassOnly(c) ? MIN_PASS_LEN : 1))
    strlcpy(c.webPass, o["webPass"], sizeof(c.webPass));
  if (!o["webPassClear"].isNull() && o["webPassClear"].as<bool>() && !ownPassOnly(c)) c.webPass[0] = '\0';
  if (!o["httpsEnabled"].isNull()) c.httpsEnabled = o["httpsEnabled"];
  if (!o["httpsRedirect"].isNull()) c.httpsRedirect = o["httpsRedirect"];
  if (!o["hitWriteEnabled"].isNull()) c.hitWriteEnabled = o["hitWriteEnabled"];
  if (!o["sunspecWriteToHit"].isNull()) c.sunspecWriteToHit = o["sunspecWriteToHit"];
  if (!o["batMaxChargePct"].isNull()) c.batMaxChargePct = o["batMaxChargePct"];
  if (!o["batMaxDischargePct"].isNull()) c.batMaxDischargePct = o["batMaxDischargePct"];
  if (!o["batMaxSoc"].isNull()) c.batMaxSoc = o["batMaxSoc"];
  if (!o["batMinSoc"].isNull()) c.batMinSoc = o["batMinSoc"];
  if (!o["apcEnabled"].isNull()) c.apcEnabled = o["apcEnabled"];
  if (!o["apcPwrLmtPct10"].isNull()) c.apcPwrLmtPct10 = o["apcPwrLmtPct10"];
  if (!o["inverterType"].isNull()) {
    const int t = o["inverterType"].as<int>();
    if (t >= 0 && t < INVERTER_TYPE_COUNT) c.inverterType = static_cast<InverterType>(t);
  }
  if (!o["hitMode"].isNull())
    c.hitMode = o["hitMode"].as<int>() == 1 ? HitLinkMode::RtuRs485 : HitLinkMode::Tcp;
  if (o["hitHost"].is<const char *>()) strlcpy(c.hitHost, o["hitHost"], sizeof(c.hitHost));
  if (!o["hitPort"].isNull()) c.hitPort = port(o["hitPort"], c.hitPort);
  if (!o["hitUnitId"].isNull()) c.hitUnitId = unitId(o["hitUnitId"], c.hitUnitId);
  if (!o["hitPollMs"].isNull()) c.hitPollMs = o["hitPollMs"];
  if (!o["hitRtuBaud"].isNull()) c.hitRtuBaud = o["hitRtuBaud"];
  if (!o["demoMode"].isNull()) c.demoMode = o["demoMode"];
  if (!o["sunspecEnabled"].isNull()) c.sunspecEnabled = o["sunspecEnabled"];
  if (!o["sunspecPort"].isNull()) c.sunspecPort = port(o["sunspecPort"], c.sunspecPort);
  if (!o["sunspecUnitId"].isNull()) c.sunspecUnitId = unitId(o["sunspecUnitId"], c.sunspecUnitId);
  if (!o["invertPowerSign"].isNull()) c.invertPowerSign = o["invertPowerSign"];
  if (!o["gatewayEnabled"].isNull()) c.gatewayEnabled = o["gatewayEnabled"];
  if (!o["gatewayPort"].isNull()) c.gatewayPort = port(o["gatewayPort"], c.gatewayPort);
  if (!o["displayEnabled"].isNull()) c.displayEnabled = o["displayEnabled"];
  if (!o["displayBrightness"].isNull()) c.displayBrightness = constrain(o["displayBrightness"].as<int>(), 0, 255);
  if (!o["displayAutoPage"].isNull()) c.displayAutoPage = o["displayAutoPage"];
  if (!o["displayPageS"].isNull()) c.displayPageMs = constrain(o["displayPageS"].as<uint32_t>(), 3u, 600u) * 1000;
  if (!o["displayTimeoutS"].isNull()) c.displayTimeoutS = constrain(o["displayTimeoutS"].as<uint32_t>(), 0u, 36000u);
  if (o["displayScheduleEnabled"].is<bool>()) c.displayScheduleEnabled = o["displayScheduleEnabled"].as<bool>();
  if (o["displayWeek"].is<JsonArrayConst>()) {
    const auto days = o["displayWeek"].as<JsonArrayConst>();
    for (size_t i = 0; i < DisplaySchedule::DAYS && i < days.size(); ++i) {
      auto &d = c.displayWeek[i];
      if (days[i]["enabled"].is<bool>()) d.enabled = days[i]["enabled"].as<bool>();
      const int start = days[i]["start"] | -1, end = days[i]["end"] | -1;
      if (start >= 0 && start < DisplaySchedule::MINUTES_PER_DAY) d.start = start;
      if (end >= 0 && end < DisplaySchedule::MINUTES_PER_DAY) d.end = end;
    }
  }
  if (!o["tasmotaEnabled"].isNull()) c.tasmotaEnabled = o["tasmotaEnabled"];
  if (!o["tasmotaLiveGrid"].isNull()) c.tasmotaLiveGrid = o["tasmotaLiveGrid"];
  if (!o["tasmotaMeterSpoof"].isNull()) c.tasmotaMeterSpoof = o["tasmotaMeterSpoof"];
  if (o["tasmotaHost"].is<const char *>()) strlcpy(c.tasmotaHost, o["tasmotaHost"], sizeof(c.tasmotaHost));
  if (!o["tasmotaPort"].isNull()) c.tasmotaPort = port(o["tasmotaPort"], c.tasmotaPort);
  if (!o["tasmotaPollMs"].isNull()) c.tasmotaPollMs = o["tasmotaPollMs"];
  if (o["tasmotaUser"].is<const char *>()) strlcpy(c.tasmotaUser, o["tasmotaUser"], sizeof(c.tasmotaUser));
  if (o["tasmotaPass"].is<const char *>() && o["tasmotaPass"].as<const char *>()[0])
    strlcpy(c.tasmotaPass, o["tasmotaPass"], sizeof(c.tasmotaPass));
  if (!o["mqttEnabled"].isNull()) c.mqttEnabled = o["mqttEnabled"];
  if (!o["mqttDiscovery"].isNull()) c.mqttDiscovery = o["mqttDiscovery"];
  if (o["mqttHost"].is<const char *>()) strlcpy(c.mqttHost, o["mqttHost"], sizeof(c.mqttHost));
  if (!o["mqttPort"].isNull()) c.mqttPort = port(o["mqttPort"], c.mqttPort);
  if (o["mqttUser"].is<const char *>()) strlcpy(c.mqttUser, o["mqttUser"], sizeof(c.mqttUser));
  if (o["mqttPass"].is<const char *>() && o["mqttPass"].as<const char *>()[0])
    strlcpy(c.mqttPass, o["mqttPass"], sizeof(c.mqttPass));
  if (o["mqttBase"].is<const char *>()) strlcpy(c.mqttBase, o["mqttBase"], sizeof(c.mqttBase));
  if (!o["mqttPublishMs"].isNull()) c.mqttPublishMs = o["mqttPublishMs"];
  if (!o["mqttIngestEnabled"].isNull()) c.mqttIngestEnabled = o["mqttIngestEnabled"];
  if (o["mqttIngestTopic"].is<const char *>()) strlcpy(c.mqttIngestTopic, o["mqttIngestTopic"], sizeof(c.mqttIngestTopic));
  if (!o["mqttIngestMaxAgeMs"].isNull()) c.mqttIngestMaxAgeMs = o["mqttIngestMaxAgeMs"];
  if (!o["cloudEnabled"].isNull()) c.cloudEnabled = o["cloudEnabled"];
  if (o["cloudHost"].is<const char *>()) strlcpy(c.cloudHost, o["cloudHost"], sizeof(c.cloudHost));
  if (o["cloudUser"].is<const char *>()) strlcpy(c.cloudUser, o["cloudUser"], sizeof(c.cloudUser));
  if (o["cloudPass"].is<const char *>() && o["cloudPass"].as<const char *>()[0])
    strlcpy(c.cloudPass, o["cloudPass"], sizeof(c.cloudPass));
  if (o["cloudStationId"].is<const char *>()) strlcpy(c.cloudStationId, o["cloudStationId"], sizeof(c.cloudStationId));
  if (!o["cloudPollMs"].isNull()) c.cloudPollMs = o["cloudPollMs"];
  if (!o["cloudMaxAgeMs"].isNull()) c.cloudMaxAgeMs = o["cloudMaxAgeMs"];
  if (!o["batEnabled"].isNull()) c.batEnabled = o["batEnabled"];
  if (!o["batHitEnabled"].isNull()) c.batHitEnabled = o["batHitEnabled"];
  if (!o["batHitPollS"].isNull()) c.batHitPollS = constrain(o["batHitPollS"].as<long>(), 30L, 3600L);
  if (!o["batCloud"].isNull()) {
    const int t = o["batCloud"].as<int>();
    if (t >= 0 && t < BATTERY_CLOUD_COUNT) c.batCloud = static_cast<BatteryCloudType>(t);
  }
  if (o["batCloudHost"].is<const char *>() && o["batCloudHost"].as<const char *>()[0])
    strlcpy(c.batCloudHost, o["batCloudHost"], sizeof(c.batCloudHost));
  if (o["batCloudAppId"].is<const char *>()) strlcpy(c.batCloudAppId, o["batCloudAppId"], sizeof(c.batCloudAppId));
  if (o["batCloudSecret"].is<const char *>() && o["batCloudSecret"].as<const char *>()[0])
    strlcpy(c.batCloudSecret, o["batCloudSecret"], sizeof(c.batCloudSecret));
  if (o["batCloudSn"].is<const char *>()) strlcpy(c.batCloudSn, o["batCloudSn"], sizeof(c.batCloudSn));
  if (!o["batCloudPollS"].isNull())
    c.batCloudPollS = constrain(o["batCloudPollS"].as<long>(), long(BAT_CLOUD_MIN_S), 3600L);
  if (!o["knxEnabled"].isNull()) c.knxEnabled = o["knxEnabled"];
  if (o["knxPa"].is<const char *>()) strlcpy(c.knxPa, o["knxPa"], sizeof(c.knxPa));
  if (!o["knxPublishMs"].isNull()) c.knxPublishMs = o["knxPublishMs"];
  if (!o["knxOnlyOnChange"].isNull()) c.knxOnlyOnChange = o["knxOnlyOnChange"];
  if (!o["knxMinDeltaW"].isNull()) c.knxMinDeltaW = o["knxMinDeltaW"];
  if (o["knxCh"].is<JsonArrayConst>()) {
    JsonArrayConst a = o["knxCh"].as<JsonArrayConst>();
    for (size_t i = 0; i < KNX_CH_COUNT && i < a.size(); i++) {
      if (!a[i]["ena"].isNull()) c.knxCh[i].ena = a[i]["ena"];
      if (a[i]["ga"].is<const char *>()) strlcpy(c.knxCh[i].ga, a[i]["ga"], sizeof(c.knxCh[i].ga));
      if (i >= KNX_DISPLAY_SWITCH) c.knxCh[i].dpt = knxMeta(i).defaultDpt;
      else if (!a[i]["dpt"].isNull()) c.knxCh[i].dpt = a[i]["dpt"];
    }
  }
  if (!o["bleLedEnabled"].isNull()) c.bleLedEnabled = o["bleLedEnabled"];
  if (o["bleLedAddr"].is<const char *>()) {
    String a = o["bleLedAddr"].as<String>();
    a.trim();
    a.toLowerCase();
    unsigned b[6];
    char tail;
    if (!a.length() || (a.length() == 17 && sscanf(a.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x%c", b, b + 1, b + 2, b + 3, b + 4, b + 5, &tail) == 6))
      strlcpy(c.bleLedAddr, a.c_str(), sizeof(c.bleLedAddr));
  }
  if (!o["bleLedAddrType"].isNull()) c.bleLedAddrType = o["bleLedAddrType"].as<int>() ? 1 : 0;
  if (o["bleLedName"].is<const char *>()) strlcpy(c.bleLedName, o["bleLedName"], sizeof(c.bleLedName));
  if (!o["bleLedVariant"].isNull()) c.bleLedVariant = uint8_t(constrain(o["bleLedVariant"].as<int>(), 0, 3));
  if (o["deviceName"].is<const char *>()) strlcpy(c.deviceName, o["deviceName"], sizeof(c.deviceName));
  if (o["uiLang"].is<const char *>()) c.uiLang = uiLangFrom(o["uiLang"]);
  if (o["uiTheme"].is<const char *>()) c.uiTheme = uiThemeFrom(o["uiTheme"]);
}
} // namespace

namespace WebConfig {

void sendGzip(AsyncWebServerRequest *req, const char *type, const uint8_t *data, size_t size) {
  auto *response = req->beginResponse(200, type, data, size);
  response->addHeader("Content-Encoding", "gzip");
  req->send(response);
}

void begin() {
  // Von außen auf HTTPS umleiten; der HTTPS-Server selbst fragt über 127.0.0.1 an.
  // Während der Einrichtung nicht, da steht man mit dem Handy im Geräte-WLAN.
  server.addMiddleware([](AsyncWebServerRequest *req, ArMiddlewareNext next) {
    const auto &c = ConfigStore::get();
    // Bis die Einrichtung fertig ist, führt jede Seite dorthin; Schnittstellen, CSS und Schriften braucht die Einrichtung selbst
    if (!c.setupComplete && req->method() == HTTP_GET) {
      const String &u = req->url();
#ifdef TLS_DIAG
      // Diagnose für die Einrichtung (selten, nur bis sie fertig ist): kommt jede Datei an, reicht der Speicher?
      Serial.printf("[Setup] GET %s%s  Heap %u, Block %u\n", req->host().c_str(), u.c_str(),
                    unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                    unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
#endif
      // Captive Portal: Prüfadressen der Handys (captive.apple.com, connectivitycheck...) auf die Einrichtung
      const String host = req->host();
      const bool viaAp = req->client() && req->client()->localIP() == WiFi.softAPIP();
      if (viaAp && host.length() && !isDigit(host[0]) && !host.endsWith(".local")) {
        req->redirect("http://" + WiFi.softAPIP().toString() + "/setup");
        return;
      }
      if (u != "/setup" && !u.startsWith("/api/") && u != "/a.css" && !u.startsWith("/f/")) {
        req->redirect("/setup");
        return;
      }
    }
    if (req->url() != "/api/tls/ca.crt" && c.httpsRedirect && c.setupComplete && HttpsServer::hasRoom() && req->client() &&
        req->client()->remoteIP() != IPAddress(127, 0, 0, 1)) {
      String host = req->host();
      const int colon = host.indexOf(':');
      if (colon >= 0) host = host.substring(0, colon);
      if (!host.length()) host = gLive.ipShown;
      req->redirect("https://" + host + req->url());
      return;
    }
    next();
  });

  // Retain the public stylesheet for clients with HTML cached from older firmware.
  server.on("/a.css", HTTP_GET, [](AsyncWebServerRequest *req) {
    sendGzip(req, "text/css", WebAssets::UI_CSS, sizeof(WebAssets::UI_CSS));
  });
  server.on("/b.js", HTTP_GET, [](AsyncWebServerRequest *req) {
    sendGzip(req, "text/javascript", WebAssets::UI_JS, sizeof(WebAssets::UI_JS));
  });
  server.on(AsyncURIMatcher::exact("/api/logout"), HTTP_POST, [](AsyncWebServerRequest *req) {
    // JSON erzwingt bei fremden Websites einen CORS-Preflight, auch ohne Fetch-Metadaten.
    if (crossSite(req) || req->contentType() != "application/json") {
      req->send(403, "text/plain", "Forbidden");
      return;
    }
    if (!requireAuth(req)) return;
    auto *r = req->beginResponse(204, "text/plain", "");
    r->addHeader("Set-Cookie", "hp_lo=1; Path=/; SameSite=Strict; HttpOnly");
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });
  server.on("/f/barlow-400.woff", HTTP_GET, [](AsyncWebServerRequest *req) {
    auto *r = req->beginResponse(200, "font/woff", FONT_BARLOW_400, FONT_BARLOW_400_LEN);
    r->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    req->send(r);
  });
  server.on("/f/barlow-600.woff", HTTP_GET, [](AsyncWebServerRequest *req) {
    auto *r = req->beginResponse(200, "font/woff", FONT_BARLOW_600, FONT_BARLOW_600_LEN);
    r->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    req->send(r);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    if (!ConfigStore::get().setupComplete) {
      req->redirect("/setup");
      return;
    }
    sendPage(req, WebAssets::DASH_HTML);
  });

  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    if (!ConfigStore::get().setupComplete) {
      req->redirect("/setup");
      return;
    }
    sendPage(req, WebAssets::INDEX_HTML);
  });

  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    sendPage(req, WebAssets::SETUP_HTML);
  });

  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    sendPage(req, WebAssets::DEBUG_HTML);
  });

  server.on(AsyncURIMatcher::exact("/api/debug"), HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    auto &c = ConfigStore::get();
    String ev = DebugLog::toJson(40);
    String out = "{";
    out += "\"uptimeMs\":"; out += millis();
    out += ",\"wMaxLimPct\":"; out += gLive.wMaxLimPct;
    out += ",\"wMaxLimEna\":"; out += gLive.wMaxLimEna ? "true" : "false";
    out += ",\"sunspecClient\":"; out += gLive.sunspecClient ? "true" : "false";
    out += ",\"hitWriteEnabled\":"; out += c.hitWriteEnabled ? "true" : "false";
    out += ",\"sunspecWriteToHit\":"; out += c.sunspecWriteToHit ? "true" : "false";
    out += ",\"count\":"; out += DebugLog::count();
    out += ",\"lastAddr\":"; out += DebugLog::lastAddr();
    out += ",\"lastVal\":"; out += DebugLog::lastValue();
    out += ",\"lastNote\":\"";
    jsonEscapeAppend(out, DebugLog::lastNote());
    out += "\"";
    out += ",\"events\":"; out += ev;
    out += "}";
    req->send(200, "application/json", out);
  });

  server.on("/api/debug/download", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    downloadLog<DebugEvt, 48>(req, "debug", DebugLog::copyTo);
  });

  server.on("/api/events/download", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    downloadLog<SysEvt, 80>(req, "events", EventLog::copyTo);
  });

  server.on("/api/debug/clear", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    DebugLog::clear();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    String out = "{\"uptimeMs\":";
    out += millis();
    out += ",\"hitPolls\":"; out += gLive.hitPolls;
    out += ",\"hitFails\":"; out += gLive.hitFails;
    out += ",\"hitErr\":\"";
    for (const char *p = HitBus::lastError(); *p; p++) if (*p != '"' && *p != '\\') out += *p;
    out += "\",\"events\":";
    out += EventLog::toJson(80);
    out += "}";
    req->send(200, "application/json", out);
  });

  server.on(AsyncURIMatcher::exact("/api/tls"), HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send(200, "application/json", HttpsServer::infoJson());
  });

  server.on("/api/tls/ca.crt", HTTP_GET, [](AsyncWebServerRequest *req) {
    const String ca = HttpsServer::caCertificate();
    if (!ca.length()) {
      req->send(404, "text/plain; charset=utf-8",
                ConfigStore::uiText("Kein Geräte-CA-Zertifikat verfügbar.", "No device CA certificate available."));
      return;
    }
    AsyncWebServerResponse *res = req->beginResponse(200, "application/x-x509-ca-cert", ca);
    res->addHeader("Content-Disposition", "attachment; filename=\"hit-proxy-ca.crt\"");
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
  });

  server.on("/api/tls/cert", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, body)) {
                req->send(400, "application/json", ConfigStore::uiText(
                    "{\"ok\":false,\"error\":\"Ungültige JSON-Daten.\"}",
                    "{\"ok\":false,\"error\":\"Invalid JSON data.\"}"));
                return;
              }
              String err;
              JsonDocument out;
              const bool en = ConfigStore::get().uiLang == UiLang::En;
              out["ok"] = HttpsServer::installCustom(doc["cert"] | "", doc["key"] | "", err, en);
              if (err.length()) out["error"] = err;
              String s;
              serializeJson(out, s);
              req->send(out["ok"] ? 200 : 400, "application/json", s);
            });

  server.on("/api/tls/self", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    HttpsServer::useSelfSigned();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    const AsyncWebParameter *p = req->getParam("secrets");
    const bool secrets = !p || p->value() != "0";
    AsyncWebServerResponse *res = req->beginResponse(200, "application/json", ConfigStore::exportBackup(secrets));
    res->addHeader("Content-Disposition", "attachment; filename=\"hit-proxy-backup.json\"");
    res->addHeader("Cache-Control", "no-store");
    req->send(res);
  });

  server.on("/api/backup/restore", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total, 32768);
              if (!body) return;
              if (!requireAuth(req)) return;
              String err;
              if (!ConfigStore::importBackup(body, err)) {
                JsonDocument out;
                out["ok"] = false;
                out["error"] = err;
                String s;
                serializeJson(out, s);
                req->send(400, "application/json", s);
                return;
              }
              EventLog::add('I', "WEB", ConfigStore::uiText(
                  "Einstellungen aus Sicherung wiederhergestellt, Neustart folgt",
                  "Settings restored from backup; restarting"));
              req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
              OtaUpdate::restartSoon();
            });

  server.on("/api/events/clear", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    EventLog::clear();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/setup/finish", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, body)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
              }
              auto &c = ConfigStore::get();
              if (ownPassOnly(c) && strlen(doc["webPass"] | "") < MIN_PASS_LEN && strlen(c.webPass) < MIN_PASS_LEN) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"password\"}");
                return;
              }
              applyJson(doc.as<JsonObjectConst>(), c);
              c.setupComplete = true;
              enforceAccess(c);
              c.sunspecPort = c.sunspecPort ? c.sunspecPort : 1502;
              c.sunspecUnitId = c.sunspecUnitId ? c.sunspecUnitId : 1;
              // Autark-Pfad: Assistent beendet AP-Zwang; LAN/Eth bleibt
              if (!doc["wifiApMode"].isNull()) c.wifiApMode = doc["wifiApMode"];
              else c.wifiApMode = false;
              ConfigStore::save();
              gLive.setupScreen = false;
              {
                // Geräte-WLAN geht gleich aus; wer darüber eingerichtet hat, braucht die Adresse im Heimnetz
                const bool eth = gLive.ethOk && strcmp(gLive.ipEth, "-") != 0;
                String out = "{\"ok\":true,\"https\":";
                out += (c.httpsEnabled && HttpsServer::running()) ? "true" : "false";
                out += ",\"ip\":\"";
                out += eth ? gLive.ipEth : gLive.ipShown;
                out += "\"}";
                req->send(200, "application/json", out);
              }
              scheduleApply(true);
            });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    auto &c = ConfigStore::get();
    doc["ipShown"] = gLive.ipShown;
    doc["setupComplete"] = c.setupComplete;
    doc["setupScreen"] = gLive.setupScreen;
    doc["ipEth"] = gLive.ipEth;
    doc["ipWifi"] = gLive.ipWifi;
    doc["hitHost"] = c.hitHost;
    doc["hitOk"] = gLive.hitOk;
    doc["hitPollsDone"] = gLive.hitPollsDone;
    doc["ethOk"] = gLive.ethOk;
    doc["wifiOk"] = gLive.wifiOk;
    doc["sunspecClient"] = gLive.sunspecClient;
    doc["resetReason"] = Watchdog::lastResetReason(ConfigStore::get().uiLang == UiLang::En);
    doc["uptimeS"] = millis() / 1000;
    doc["heapInternal"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    doc["heapInternalMin"] = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    doc["https"] = HttpsServer::running();
    doc["gatewayEnabled"] = ConfigStore::get().gatewayEnabled;
    doc["gatewayClients"] = gLive.gatewayClients;
    doc["hitLink"] = ConfigStore::get().hitMode == HitLinkMode::RtuRs485 ? "RS485" : "TCP";
    doc["mqttOk"] = gLive.mqttOk;
    doc["mqttIngestOk"] = gLive.mqttIngestOk;
    doc["cloudOk"] = gLive.cloudOk;
    doc["knxOk"] = gLive.knxOk;
    doc["dataOrigin"] = dataOriginLabel(gLive.dataOrigin);
    doc["pvW"] = gLive.pvW;
    doc["pv1W"] = gLive.pv1W; doc["pv2W"] = gLive.pv2W; doc["pv3W"] = gLive.pv3W; doc["pv4W"] = gLive.pv4W;
    doc["pv1V"] = gLive.pv1V; doc["pv2V"] = gLive.pv2V; doc["pv3V"] = gLive.pv3V; doc["pv4V"] = gLive.pv4V;
    doc["pv1A"] = gLive.pv1A; doc["pv2A"] = gLive.pv2A; doc["pv3A"] = gLive.pv3A; doc["pv4A"] = gLive.pv4A;
    doc["batW"] = gLive.batW;
    doc["batV"] = gLive.batV;
    doc["batA"] = gLive.batA;
    doc["soc"] = gLive.soc;
    doc["soh"] = gLive.soh;
    doc["gridW"] = gLive.gridW;
    doc["gridW1"] = gLive.gridW1; doc["gridW2"] = gLive.gridW2; doc["gridW3"] = gLive.gridW3;
    doc["v1"] = gLive.v1;
    doc["v2"] = gLive.v2;
    doc["v3"] = gLive.v3;
    doc["i1"] = gLive.i1;
    doc["i2"] = gLive.i2;
    doc["i3"] = gLive.i3;
    doc["freqHz"] = gLive.freqHz;
    doc["energyPvKwh"] = gLive.energyPvKwh;
    doc["energyImpKwh"] = gLive.energyImpKwh;
    doc["energyExpKwh"] = gLive.energyExpKwh;
    doc["tasmotaOk"] = gLive.tasmotaOk;
    doc["tasmotaLiveGrid"] = c.tasmotaLiveGrid;
    doc["tasmotaPowerW"] = gLive.tasmotaPowerW;
    doc["tasmotaTotalKwh"] = gLive.tasmotaTotalKwh;
    doc["tasmotaExportKwh"] = gLive.tasmotaExportKwh;
    doc["tasmotaSensorKey"] = gLive.tasmotaSensorKey;
    doc["wMaxLimPct"] = gLive.wMaxLimPct;
    doc["wMaxLimEna"] = gLive.wMaxLimEna;
    doc["lastWriteNote"] = DebugLog::lastNote();
    doc["lastHitAgeMs"] = gLive.lastHitMs ? (millis() - gLive.lastHitMs) : 0;
    doc["status"] = gLive.status;
    doc["version"] = PROXY_VERSION;
    doc["uiLang"] = uiLangCode(c.uiLang);
    doc["uiTheme"] = uiThemeCode(c.uiTheme);
    doc["otaBusy"] = OtaUpdate::inProgress();
    doc["demoMode"] = c.demoMode;
    {
      // gewählte Funktionen: die Oberfläche blendet Menüpunkte nicht genutzter Dienste aus
      JsonObject f = doc["features"].to<JsonObject>();
      f["sunspec"] = c.sunspecEnabled;
      f["mqtt"] = c.mqttEnabled || c.mqttIngestEnabled;
      f["gateway"] = c.gatewayEnabled;
      f["knx"] = c.knxEnabled;
      f["tasmota"] = c.tasmotaEnabled;
      f["cloud"] = c.cloudEnabled;
      f["akku"] = c.batEnabled;
    }
    doc["board"] = BOARD_NAME;
    doc["hasDisplay"] = BOARD_HAS_DISPLAY;
    doc["hasBleLed"] = BleLed::SUPPORTED;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    auto &c = ConfigStore::get();
    JsonDocument doc;
    doc["useEthernet"] = c.useEthernet;
    doc["wifiEnabled"] = c.wifiEnabled;
    doc["wifiApMode"] = c.wifiApMode;
    doc["setupComplete"] = c.setupComplete;
    doc["wifiSsid"] = c.wifiSsid;
    doc["staSsid"] = c.staSsid;
    doc["staPass"] = "";
    doc["staPassSet"] = c.staPass[0] != 0;
    // UI zeigt den Display-Code; ohne Display ist es das eigene Passwort und wird nicht ausgeliefert
    doc["wifiPass"] = c.displayCode[0] ? c.displayCode : (ownPassOnly(c) ? "" : c.wifiPass);
    doc["ownPassOnly"] = ownPassOnly(c);
    doc["webUser"] = c.webUser;
    doc["webPass"] = "";
    doc["webPassSet"] = c.webPass[0] != 0;
    doc["httpsEnabled"] = c.httpsEnabled;
    doc["httpsRedirect"] = c.httpsRedirect;
    doc["displayCode"] = c.displayCode;
    doc["hitWriteEnabled"] = c.hitWriteEnabled;
    doc["sunspecWriteToHit"] = c.sunspecWriteToHit;
    doc["batMaxChargePct"] = c.batMaxChargePct;
    doc["batMaxDischargePct"] = c.batMaxDischargePct;
    doc["batMaxSoc"] = c.batMaxSoc;
    doc["batMinSoc"] = c.batMinSoc;
    doc["apcEnabled"] = c.apcEnabled;
    doc["apcPwrLmtPct10"] = c.apcPwrLmtPct10;
    doc["hitMode"] = int(c.hitMode);
    doc["inverterType"] = int(c.inverterType);
    doc["hitHost"] = c.hitHost;
    doc["hitPort"] = c.hitPort;
    doc["hitUnitId"] = c.hitUnitId;
    doc["hitPollMs"] = c.hitPollMs;
    doc["hitRtuBaud"] = c.hitRtuBaud;
    doc["demoMode"] = c.demoMode;
    doc["sunspecEnabled"] = c.sunspecEnabled;
    doc["sunspecPort"] = c.sunspecPort;
    doc["sunspecUnitId"] = c.sunspecUnitId;
    doc["invertPowerSign"] = c.invertPowerSign;
    doc["gatewayEnabled"] = c.gatewayEnabled;
    doc["gatewayPort"] = c.gatewayPort;
    doc["displayEnabled"] = c.displayEnabled;
    doc["displayBrightness"] = c.displayBrightness;
    doc["displayPageS"] = c.displayPageMs / 1000;
    doc["displayAutoPage"] = c.displayAutoPage;
    doc["displayTimeoutS"] = c.displayTimeoutS;
    doc["displayScheduleEnabled"] = c.displayScheduleEnabled;
    JsonArray displayWeek = doc["displayWeek"].to<JsonArray>();
    for (const auto &d : c.displayWeek) {
      JsonObject day = displayWeek.add<JsonObject>();
      day["enabled"] = d.enabled;
      day["start"] = d.start;
      day["end"] = d.end;
    }
    doc["tasmotaEnabled"] = c.tasmotaEnabled;
    doc["tasmotaLiveGrid"] = c.tasmotaLiveGrid;
    doc["tasmotaMeterSpoof"] = c.tasmotaMeterSpoof;
    doc["tasmotaHost"] = c.tasmotaHost;
    doc["tasmotaPort"] = c.tasmotaPort;
    doc["tasmotaPollMs"] = c.tasmotaPollMs;
    doc["tasmotaUser"] = c.tasmotaUser;
    doc["tasmotaPass"] = "";
    doc["tasmotaPassSet"] = c.tasmotaPass[0] != 0;
    doc["mqttEnabled"] = c.mqttEnabled;
    doc["mqttDiscovery"] = c.mqttDiscovery;
    doc["mqttHost"] = c.mqttHost;
    doc["mqttPort"] = c.mqttPort;
    doc["mqttUser"] = c.mqttUser;
    doc["mqttPass"] = "";
    doc["mqttPassSet"] = c.mqttPass[0] != 0;
    doc["mqttBase"] = c.mqttBase;
    doc["mqttPublishMs"] = c.mqttPublishMs;
    doc["mqttIngestEnabled"] = c.mqttIngestEnabled;
    doc["mqttIngestTopic"] = c.mqttIngestTopic;
    doc["mqttIngestMaxAgeMs"] = c.mqttIngestMaxAgeMs;
    doc["cloudEnabled"] = c.cloudEnabled;
    doc["cloudHost"] = c.cloudHost;
    doc["cloudUser"] = c.cloudUser;
    doc["cloudPass"] = "";
    doc["cloudPassSet"] = c.cloudPass[0] != 0;
    doc["cloudStationId"] = c.cloudStationId;
    doc["cloudPollMs"] = c.cloudPollMs;
    doc["cloudMaxAgeMs"] = c.cloudMaxAgeMs;
    doc["batEnabled"] = c.batEnabled;
    doc["batHitEnabled"] = c.batHitEnabled;
    doc["batHitPollS"] = c.batHitPollS;
    doc["batCloud"] = int(c.batCloud);
    doc["batCloudHost"] = c.batCloudHost;
    doc["batCloudAppId"] = c.batCloudAppId;
    doc["batCloudSecret"] = "";
    doc["batCloudSecretSet"] = c.batCloudSecret[0] != 0;
    doc["batCloudSn"] = c.batCloudSn;
    doc["batCloudPollS"] = c.batCloudPollS;
    doc["batCloudMinS"] = BAT_CLOUD_MIN_S;
    doc["knxEnabled"] = c.knxEnabled;
    doc["knxPa"] = c.knxPa;
    doc["knxPublishMs"] = c.knxPublishMs;
    doc["knxOnlyOnChange"] = c.knxOnlyOnChange;
    doc["knxMinDeltaW"] = c.knxMinDeltaW;
    doc["bleLedEnabled"] = c.bleLedEnabled;
    doc["bleLedAddr"] = c.bleLedAddr;
    doc["bleLedAddrType"] = c.bleLedAddrType;
    doc["bleLedName"] = c.bleLedName;
    doc["bleLedVariant"] = c.bleLedVariant;
    JsonArray ch = doc["knxCh"].to<JsonArray>();
    for (size_t i = 0; i < KNX_CH_COUNT; i++) {
      JsonObject o = ch.add<JsonObject>();
      o["ena"] = c.knxCh[i].ena;
      o["ga"] = c.knxCh[i].ga;
      o["dpt"] = i < KNX_DISPLAY_SWITCH && c.knxCh[i].dpt ? c.knxCh[i].dpt : knxMeta(i).defaultDpt;
    }
    doc["deviceName"] = c.deviceName;
    doc["uiLang"] = uiLangCode(c.uiLang);
    doc["uiTheme"] = uiThemeCode(c.uiTheme);
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on(AsyncURIMatcher::exact("/api/bleled"), HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    BleLed::status(doc.to<JsonObject>());
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/bleled/scan", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    BleLed::startScan();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/bleled/test", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    BleLed::setPower(req->hasParam("on") && req->getParam("on")->value() == "1");
    req->send(200, "application/json", "{\"ok\":true}");
  });

  // Bedienung von der Seite "Bluetooth": on=0|1, bri=1..100 (0 = aus), rgb=rrggbb
  server.on("/api/bleled/set", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    if (req->hasParam("on")) BleLed::setPower(req->getParam("on")->value() == "1");
    if (req->hasParam("rgb")) {
      const String v = req->getParam("rgb")->value();
      char *end = nullptr;
      const unsigned long rgb = strtoul(v.c_str(), &end, 16);
      if (v.length() != 6 || *end) {
        req->send(400, "application/json", "{\"ok\":false}");
        return;
      }
      BleLed::setColor(rgb >> 16, (rgb >> 8) & 0xff, rgb & 0xff);
    }
    if (req->hasParam("bri")) BleLed::setBrightness(constrain(req->getParam("bri")->value().toInt(), 0, 100));
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/system", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    sendPage(req, WebAssets::SYS_HTML);
  });

  server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    const auto &c = ConfigStore::get();
    JsonDocument doc;
    SysInfo::fill(doc.to<JsonObject>());
    doc["board"] = BOARD_NAME;
    doc["hasDisplay"] = BOARD_HAS_DISPLAY;
    doc["hasBleLed"] = BleLed::SUPPORTED;
    doc["version"] = PROXY_VERSION;
    doc["uptimeS"] = millis() / 1000;
    doc["resetReason"] = Watchdog::lastResetReason(c.uiLang == UiLang::En);
    doc["ipEth"] = gLive.ipEth;
    doc["ipWifi"] = gLive.ipWifi;
    doc["mac"] = Network.macAddress();
    doc["https"] = HttpsServer::running();
    JsonObject f = doc["features"].to<JsonObject>();
    f["sunspec"] = c.sunspecEnabled;
    f["mqtt"] = c.mqttEnabled || c.mqttIngestEnabled;
    f["gateway"] = c.gatewayEnabled;
    f["knx"] = c.knxEnabled;
    f["tasmota"] = c.tasmotaEnabled;
    f["cloud"] = c.cloudEnabled;
    f["akku"] = c.batEnabled;
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

#ifdef HAS_BLE_LED
  server.on("/bluetooth", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    sendPage(req, WebAssets::BLE_HTML);
  });
#endif

  server.on("/api/knx/meta", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    JsonArray pts = doc["points"].to<JsonArray>();
    static const char *kinds[] = {"PowerW", "Percent", "Volt", "Amp", "Hz", "EnergyKwh", "RatioPct",
                                   "Temp", "MilliVolt", "Count", "Flag", "Switch", "Scaling", "Rgb"};
    for (size_t i = 0; i < KNX_CH_COUNT; i++) {
      const auto &m = knxMeta(i);
      JsonObject o = pts.add<JsonObject>();
      // ausgeblendete Zeilen bleiben in der Liste, damit die Position zur gespeicherten Zuordnung passt
      if ((i == KNX_DISPLAY_SWITCH && !HAS_DISPLAY) || (i >= KNX_LED_SWITCH && !BleLed::SUPPORTED)) o["hide"] = true;
      o["id"] = m.id;
      o["label"] = m.label;
      o["labelEn"] = m.labelEn;
      o["dpt"] = m.defaultDpt;
      o["kind"] = kinds[uint8_t(m.kind)];
    }
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              JsonDocument doc;
              if (deserializeJson(doc, body)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
              }
              auto &c = ConfigStore::get();
              applyJson(doc.as<JsonObjectConst>(), c);
              // Speichern = Setup erledigt (QR-Screen aus), außer explizit false
              if (doc["setupComplete"].isNull()) c.setupComplete = true;
              enforceAccess(c);
              ConfigStore::save();
              gLive.setupScreen = !c.setupComplete;
              req->send(200, "application/json", "{\"ok\":true}");
              scheduleApply(false);
            });

  server.on("/api/control/apply", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    ctrlDone = false;
    ctrlOk = false;
    ctrlErr[0] = '\0';
    addWork(W_CTRL);
    req->send(200, "application/json", "{\"ok\":true,\"pending\":true,\"done\":false}");
  });

  server.on("/api/control/result", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument out;
    out["pending"] = !ctrlDone;
    out["done"] = ctrlDone;
    out["ok"] = ctrlDone && ctrlOk;
    out["error"] = ctrlErr;
    String s; serializeJson(out, s);
    req->send(200, "application/json", s);
  });

  server.on("/api/battery", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    BatteryData::toJson(doc, ConfigStore::get());
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    sendPage(req, WebAssets::HISTORY_HTML);
  });

  server.on("/api/history/day", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    JsonDocument doc;
    History::day(doc);
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/history/download", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    History::downloadCsv(req);
  });

  server.on("/api/history/bars", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    const String unit = req->hasParam("unit") ? req->getParam("unit")->value() : String("day");
    const int n = req->hasParam("n") ? constrain(req->getParam("n")->value().toInt(), 1, 400) : 31;
    JsonDocument doc;
    History::bars(unit.c_str(), n, doc);
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  server.on("/api/screen", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    size_t len = 0;
    int w = 0, h = 0;
    const uint8_t *fb = StatusDisplay::frame(len, w, h);
    if (!fb) {
      req->send(503, "text/plain", "kein Bildpuffer");
      return;
    }
    auto *r = req->beginResponse("application/octet-stream", len,
                                 [fb, len](uint8_t *buf, size_t maxLen, size_t index) -> size_t {
                                   size_t n = min(maxLen, len - index);
                                   memcpy(buf, fb + index, n);
                                   if (index + n >= len) StatusDisplay::frameDone();
                                   return n;
                                 });
    r->addHeader("X-Width", String(w));
    r->addHeader("X-Height", String(h));
    r->addHeader("Cache-Control", "no-store");
    req->send(r);
  });

  server.on("/api/access/new-code", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    if (ownPassOnly(ConfigStore::get())) { // ohne Display könnte den neuen Code niemand ablesen
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"no display\"}");
      return;
    }
    ConfigStore::newDisplayCode();
    addWork(W_NEWCODE); // NetBoot + Display in der Hauptschleife (SPI!)
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/display/tap", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    StatusDisplay::requestTap();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/poll", HTTP_POST, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    addWork(W_POLL);
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/tasmota/probe", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              if (!requireAuth(req)) return;
              if (!probeLock) {
                req->send(503, "application/json", "{\"ok\":false,\"pending\":false}");
                return;
              }
              JsonDocument in;
              deserializeJson(in, body);
              xSemaphoreTake(probeLock, portMAX_DELAY);
              if (probeState == ProbeState::Queued || probeState == ProbeState::Running) {
                xSemaphoreGive(probeLock);
                req->send(409, "application/json", "{\"ok\":false,\"pending\":true,\"summary\":\"Probe läuft bereits\"}");
                return;
              }
              auto &c = ConfigStore::get();
              probeReq = c;
              if (in["tasmotaHost"].is<const char *>())
                strlcpy(probeReq.tasmotaHost, in["tasmotaHost"], sizeof(probeReq.tasmotaHost));
              if (!in["tasmotaPort"].isNull()) probeReq.tasmotaPort = in["tasmotaPort"];
              if (in["tasmotaUser"].is<const char *>())
                strlcpy(probeReq.tasmotaUser, in["tasmotaUser"], sizeof(probeReq.tasmotaUser));
              if (in["tasmotaPass"].is<const char *>() && in["tasmotaPass"].as<const char *>()[0])
                strlcpy(probeReq.tasmotaPass, in["tasmotaPass"], sizeof(probeReq.tasmotaPass));
              probeReq.tasmotaEnabled = true;
              probeState = ProbeState::Queued;
              xSemaphoreGive(probeLock);
              req->send(200, "application/json", "{\"ok\":true,\"pending\":true}");
            });

  // Cloud-Zugangsdaten prüfen, ohne sie zu speichern; leere Passwortfelder nehmen den gespeicherten Wert
  server.on("/api/cloud/test", HTTP_POST, [](AsyncWebServerRequest *req) {}, NULL,
            [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total) {
              const char *body = fullBody(req, data, len, index, total);
              if (!body) return;
              JsonDocument in;
              if (deserializeJson(in, body)) {
                req->send(400, "application/json", "{\"ok\":false,\"error\":\"bad json\"}");
                return;
              }
              static AppConfig t; // groß, nicht auf den Stack der Web-Task
              t = ConfigStore::get();
              applyJson(in.as<JsonObjectConst>(), t);
              const bool bat = !strcmp(in["kind"] | "", "akku");
              HoyCloud::startTest(bat ? HoyCloud::Test::Battery : HoyCloud::Test::Hoymiles, t);
              req->send(200, "application/json", "{\"ok\":true,\"pending\":true}");
            });

  server.on("/api/cloud/test/result", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    String msg, station;
    const uint8_t st = HoyCloud::testState(msg, station);
    JsonDocument out;
    out["pending"] = st == 1;
    out["ok"] = st == 2;
    out["msg"] = msg;
    out["station"] = station;
    String s; serializeJson(out, s);
    req->send(200, "application/json", s);
  });

  server.on("/api/tasmota/probe/result", HTTP_GET, [](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    String result;
    if (probeLock) {
      xSemaphoreTake(probeLock, portMAX_DELAY);
      if (probeState == ProbeState::Done) result = probeResult;
      xSemaphoreGive(probeLock);
    }
    req->send(200, "application/json", result.length() ? result : "{\"pending\":true,\"ok\":false}");
  });

  OtaUpdate::registerRoutes(server, requireAuth);

  probeLock = xSemaphoreCreateMutex();
  server.begin();
}

void loop() {
  runPendingApply();
  runDeferredWork();
  runProbe();
  SysInfo::loop();
  OtaUpdate::loop();
  HttpsServer::loop();
}

} // namespace WebConfig
