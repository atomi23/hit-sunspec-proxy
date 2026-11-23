#include "TasmotaClient.h"
#include "LiveData.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
struct Endpoint {
  char host[sizeof(AppConfig::tasmotaHost)];
  char user[sizeof(AppConfig::tasmotaUser)];
  char pass[sizeof(AppConfig::tasmotaPass)];
  uint16_t port;
};

Endpoint endpoint(const AppConfig &cfg) {
  Endpoint ep{};
  strlcpy(ep.host, cfg.tasmotaHost, sizeof(ep.host));
  strlcpy(ep.user, cfg.tasmotaUser, sizeof(ep.user));
  strlcpy(ep.pass, cfg.tasmotaPass, sizeof(ep.pass));
  ep.port = cfg.tasmotaPort;
  return ep;
}

Endpoint configured, probeEndpoint;
uint32_t pollMs = 0;
uint32_t nextPoll = 0;
bool enabled = false, forcePoll = false, probeQueued = false, probeEnglish = false;

// Ein Auftrag zur Zeit; die Tasmota-Task greift nicht auf gLive zu.
TaskHandle_t worker = nullptr;
enum class State { Idle, Running, Done };
std::atomic<State> state{State::Idle};
Endpoint job;
bool jobIsProbe = false, discardPoll = false;
String raw;
JsonDocument reply;
enum class Reply { HttpError, JsonError, Parsed };
Reply status;

// Zugangsdaten stehen in der Query: & # % + Leerzeichen usw. müssen kodiert werden
String urlEncode(const char *s) {
  static const char hex[] = "0123456789ABCDEF";
  String out;
  for (; *s; s++) {
    const uint8_t c = uint8_t(*s);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += char(c);
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

String buildUrl(const Endpoint &ep) {
  String url = String("http://") + ep.host + ":" + ep.port + "/cm?";
  if (ep.user[0] && ep.pass[0]) {
    url += "user=" + urlEncode(ep.user) + "&password=" + urlEncode(ep.pass) + "&";
  }
  url += "cmnd=Status%208";
  return url;
}

bool parseEnergy(JsonVariantConst root) {
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

  // Meter_Number kommt je nach Skript als Text oder als Zahl; as<const char*>() wäre bei Zahl nullptr
  JsonVariantConst meter = energy["Meter_Number"];
  if (meter.is<const char *>()) {
    strlcpy(gLive.tasmotaMeterId, meter.as<const char *>(), sizeof(gLive.tasmotaMeterId));
  } else if (meter.is<double>()) {
    snprintf(gLive.tasmotaMeterId, sizeof(gLive.tasmotaMeterId), "%.0f", meter.as<double>());
  }

  return true;
}

bool fetchStatus8(const Endpoint &ep, String &bodyOut) {
  HTTPClient http;
  const String url = buildUrl(ep);
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

void taskMain(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (state.load(std::memory_order_acquire) != State::Running) continue;
    String body;
    status = !fetchStatus8(job, body) ? Reply::HttpError
             : deserializeJson(reply, body) ? Reply::JsonError : Reply::Parsed;
    if (jobIsProbe) raw = std::move(body);
    state.store(State::Done, std::memory_order_release);
  }
}

void startJob(const Endpoint &ep, bool probe) {
  job = ep;
  jobIsProbe = probe;
  discardPoll = false;
  if (!worker && xTaskCreatePinnedToCore(taskMain, "tasmota", 8192, nullptr, 1, &worker, 0) != pdPASS) {
    status = Reply::HttpError;
    state.store(State::Done, std::memory_order_release);
    return;
  }
  state.store(State::Running, std::memory_order_release);
  xTaskNotifyGive(worker);
}

bool publishReply() {
  gLive.tasmotaOk = status == Reply::Parsed && parseEnergy(reply.as<JsonVariantConst>());
  if (gLive.tasmotaOk) gLive.lastTasmotaMs = millis();
  return gLive.tasmotaOk;
}

void finishJob() {
  reply.clear();
  state.store(State::Idle, std::memory_order_release);
}
} // namespace

namespace TasmotaClient {

void begin(const AppConfig &cfg) {
  applyConfig(cfg);
}

void applyConfig(const AppConfig &cfg) {
  configured = endpoint(cfg);
  enabled = cfg.tasmotaEnabled;
  pollMs = max<uint32_t>(2000, cfg.tasmotaPollMs);
  nextPoll = millis();
  discardPoll = !jobIsProbe && state.load(std::memory_order_relaxed) != State::Idle;
  if (!enabled || discardPoll) gLive.tasmotaOk = false;
}

void pollNow() {
  if (enabled) forcePoll = true;
  else gLive.tasmotaOk = false;
}

void requestProbe(const AppConfig &cfg) {
  probeEndpoint = endpoint(cfg);
  probeEnglish = cfg.uiLang == UiLang::En;
  probeQueued = true;
}

bool takeProbeResult(String &rawOut, String &summaryOut, bool &ok) {
  if (state.load(std::memory_order_acquire) != State::Done || !jobIsProbe) return false;
  rawOut = std::move(raw);
  const bool english = probeEnglish;
  ok = publishReply();
  if (status == Reply::HttpError) {
    summaryOut = english ? "HTTP request failed. Check the IP address, network and password."
                         : "HTTP fehlgeschlagen — IP/Netz/Passwort prüfen";
  } else if (status == Reply::JsonError) {
    summaryOut = english ? "Invalid JSON" : "JSON ungültig";
  } else if (!ok) {
    summaryOut = english ? "No meter block found (Power/E_in/E_out/Total)"
                         : "Kein Zähler-Block gefunden (Power/E_in/E_out/Total)";
  } else {
    char buf[192];
    snprintf(buf, sizeof(buf),
             english ? "OK · Sensor %s · Power %ld W · Import %.3f kWh · Export %.3f kWh%s%s"
                     : "OK · Sensor %s · Power %ld W · Bezug %.3f kWh · Einsp. %.3f kWh%s%s",
             gLive.tasmotaSensorKey,
             long(gLive.tasmotaPowerW),
             gLive.tasmotaTotalKwh,
             gLive.tasmotaExportKwh,
             gLive.tasmotaMeterId[0] ? (english ? " · No. " : " · Nr ") : "",
             gLive.tasmotaMeterId[0] ? gLive.tasmotaMeterId : "");
    summaryOut = buf;
  }
  finishJob();
  return true;
}

void loop() {
  const auto s = state.load(std::memory_order_acquire);
  if (s == State::Running) return;
  if (s == State::Done) {
    if (jobIsProbe) return;
    if (!discardPoll) publishReply();
    finishJob();
  }
  if (probeQueued) {
    probeQueued = false;
    startJob(probeEndpoint, true);
  } else if (enabled && (forcePoll || int32_t(millis() - nextPoll) >= 0)) {
    forcePoll = false;
    nextPoll = millis() + pollMs;
    startJob(configured, false);
  }
}

} // namespace TasmotaClient
