#include "AppConfig.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include <nvs.h>

namespace {
Preferences prefs;
AppConfig cfg;
const char *NS = "hitproxy";

void randomPass(char *out, size_t n) {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  const size_t alen = sizeof(alphabet) - 1;
  size_t len = (n > 9) ? 8 : (n - 1);
  for (size_t i = 0; i < len; i++) out[i] = alphabet[esp_random() % alen];
  out[len] = '\0';
}

// Zugangsdaten; ohne Haken beim Sichern fehlen sie in der Datei und bleiben beim Wiederherstellen unverändert
const char *const SECRET_KEYS[] = {"wpass", "stapass", "webpass", "dcode", "taspass", "mqpass", "cldpass", "batcsec"};

bool isSecret(const char *key) {
  for (const char *k : SECRET_KEYS)
    if (strcmp(k, key) == 0) return true;
  return false;
}

enum class EntryType : uint8_t { Bad, U8, I8, U16, I16, U32, I32, U64, I64, Str };

EntryType entryType(const char *t) {
  static const struct { const char *name; EntryType type; } names[] = {
      {"u8", EntryType::U8},   {"i8", EntryType::I8},   {"u16", EntryType::U16}, {"i16", EntryType::I16},
      {"u32", EntryType::U32}, {"i32", EntryType::I32}, {"u64", EntryType::U64}, {"i64", EntryType::I64},
      {"str", EntryType::Str}};
  for (const auto &n : names)
    if (strcmp(n.name, t) == 0) return n.type;
  return EntryType::Bad;
}

bool valueFits(EntryType t, JsonVariantConst v) {
  switch (t) {
  case EntryType::U8: return v.is<uint8_t>();
  case EntryType::I8: return v.is<int8_t>();
  case EntryType::U16: return v.is<uint16_t>();
  case EntryType::I16: return v.is<int16_t>();
  case EntryType::U32: return v.is<uint32_t>();
  case EntryType::I32: return v.is<int32_t>();
  case EntryType::U64: return v.is<uint64_t>();
  case EntryType::I64: return v.is<int64_t>();
  case EntryType::Str: return v.is<const char *>() && strlen(v.as<const char *>()) <= ConfigStore::BACKUP_MAX_STR;
  default: return false;
  }
}

bool writeEntry(const char *k, EntryType t, JsonVariantConst v) {
  switch (t) {
  case EntryType::U8: return prefs.putUChar(k, v.as<uint8_t>());
  case EntryType::I8: return prefs.putChar(k, v.as<int8_t>());
  case EntryType::U16: return prefs.putUShort(k, v.as<uint16_t>());
  case EntryType::I16: return prefs.putShort(k, v.as<int16_t>());
  case EntryType::U32: return prefs.putUInt(k, v.as<uint32_t>());
  case EntryType::I32: return prefs.putInt(k, v.as<int32_t>());
  case EntryType::U64: return prefs.putULong64(k, v.as<uint64_t>());
  case EntryType::I64: return prefs.putLong64(k, v.as<int64_t>());
  case EntryType::Str: {
    const char *str = v.as<const char *>();
    return prefs.putString(k, str) == strlen(str);
  }
  default: return false;
  }
}
}

namespace ConfigStore {

void resetDefaults() { cfg = AppConfig{}; }

const char *newDisplayCode() {
  randomPass(cfg.displayCode, sizeof(cfg.displayCode));
  strlcpy(cfg.wifiPass, cfg.displayCode, sizeof(cfg.wifiPass)); // Geräte-WLAN = Zugangscode
  save();
  return cfg.displayCode;
}

void factoryReset() {
  prefs.clear();
  resetDefaults();
}

AppConfig &get() { return cfg; }

void provisionFirstBoot() {
  resetDefaults();
  uint64_t mac = ESP.getEfuseMac();
  uint16_t tail = uint16_t(mac & 0xffff);
  snprintf(cfg.wifiSsid, sizeof(cfg.wifiSsid), "HIT-Proxy-%04X", tail);
  if (HAS_DISPLAY) {
    randomPass(cfg.displayCode, sizeof(cfg.displayCode));
    strlcpy(cfg.wifiPass, cfg.displayCode, sizeof(cfg.wifiPass)); // AP = Display-Code
  } else {
    strlcpy(cfg.wifiPass, DEFAULT_PASS, sizeof(cfg.wifiPass));
  }
  strlcpy(cfg.webUser, "admin", sizeof(cfg.webUser));
  cfg.webPass[0] = '\0'; // selbst wählen nach Login mit Display-Code
  cfg.wifiApMode = true;
  cfg.wifiEnabled = true;
  cfg.setupComplete = false;
  cfg.hitWriteEnabled = false;
  snprintf(cfg.deviceName, sizeof(cfg.deviceName), "HIT-%04X", tail);
}

void begin() {
  prefs.begin(NS, false);
  if (!prefs.isKey("ok")) {
    provisionFirstBoot();
    save();
    Serial.printf("First-Boot: AP \"%s\" Passwort \"%s\" (Web-Pass leer)\n", cfg.wifiSsid, cfg.wifiPass);
  } else {
    load();
    if (!HAS_DISPLAY) {
      // vor v0.5.13 geflasht, aber nie eingerichtet: der Zufallscode war ohne Display nicht ablesbar
      if (!cfg.setupComplete && !cfg.webPass[0] && (cfg.displayCode[0] || strcmp(cfg.wifiPass, DEFAULT_PASS))) {
        cfg.displayCode[0] = '\0';
        strlcpy(cfg.wifiPass, DEFAULT_PASS, sizeof(cfg.wifiPass));
        save();
      }
    } else if (!cfg.displayCode[0]) {
      randomPass(cfg.displayCode, sizeof(cfg.displayCode));
      if (!cfg.wifiPass[0] || strcmp(cfg.wifiPass, "hitproxy1") == 0)
        strlcpy(cfg.wifiPass, cfg.displayCode, sizeof(cfg.wifiPass));
      save();
    }
  }
}

bool load() {
  if (!prefs.isKey("ok")) {
    provisionFirstBoot();
    return false;
  }
  cfg.useEthernet = prefs.getBool("eth", cfg.useEthernet);
  cfg.wifiEnabled = prefs.getBool("wifi", cfg.wifiEnabled);
  cfg.wifiApMode = prefs.getBool("ap", cfg.wifiApMode);
  cfg.setupComplete = prefs.getBool("setup", cfg.setupComplete);
  strlcpy(cfg.wifiSsid, prefs.getString("ssid", cfg.wifiSsid).c_str(), sizeof(cfg.wifiSsid));
  strlcpy(cfg.wifiPass, prefs.getString("wpass", cfg.wifiPass).c_str(), sizeof(cfg.wifiPass));
  strlcpy(cfg.staSsid, prefs.getString("stassid", cfg.staSsid).c_str(), sizeof(cfg.staSsid));
  strlcpy(cfg.staPass, prefs.getString("stapass", cfg.staPass).c_str(), sizeof(cfg.staPass));
  strlcpy(cfg.webUser, prefs.getString("webuser", cfg.webUser).c_str(), sizeof(cfg.webUser));
  strlcpy(cfg.webPass, prefs.getString("webpass", cfg.webPass).c_str(), sizeof(cfg.webPass));
  cfg.httpsEnabled = prefs.getBool("httpsena", cfg.httpsEnabled);
  cfg.httpsRedirect = prefs.getBool("httpsred", cfg.httpsRedirect);
  strlcpy(cfg.displayCode, prefs.getString("dcode", cfg.displayCode).c_str(), sizeof(cfg.displayCode));

  {
    const uint8_t inv = prefs.getUChar("invtype", 0);
    cfg.inverterType = static_cast<InverterType>(inv < INVERTER_TYPE_COUNT ? inv : 0);
  }
  cfg.hitMode = static_cast<HitLinkMode>(prefs.getUChar("hitmode", static_cast<uint8_t>(cfg.hitMode)));
  strlcpy(cfg.hitHost, prefs.getString("hithost", cfg.hitHost).c_str(), sizeof(cfg.hitHost));
  cfg.hitPort = prefs.getUShort("hitport", cfg.hitPort);
  cfg.hitUnitId = prefs.getUChar("hituid", cfg.hitUnitId);
  cfg.hitPollMs = prefs.getUInt("hitpoll", cfg.hitPollMs);
  cfg.hitRtuBaud = prefs.getUInt("hitbaud", cfg.hitRtuBaud);
  cfg.demoMode = prefs.getBool("demo", cfg.demoMode);

  cfg.sunspecEnabled = prefs.getBool("ssena", cfg.sunspecEnabled);
  cfg.sunspecPort = prefs.getUShort("ssport", cfg.sunspecPort);
  cfg.sunspecUnitId = prefs.getUChar("ssuid", cfg.sunspecUnitId);
  cfg.invertPowerSign = prefs.getBool("invpwr", cfg.invertPowerSign);
  cfg.gatewayEnabled = prefs.getBool("gwena", cfg.gatewayEnabled);
  cfg.gatewayPort = prefs.getUShort("gwport", cfg.gatewayPort);

  cfg.hitWriteEnabled = prefs.getBool("hwrena", cfg.hitWriteEnabled);
  cfg.sunspecWriteToHit = prefs.getBool("ss2hit", cfg.sunspecWriteToHit);
  cfg.batMaxChargePct = prefs.getUShort("batc", cfg.batMaxChargePct);
  cfg.batMaxDischargePct = prefs.getUShort("batd", cfg.batMaxDischargePct);
  cfg.batMaxSoc = prefs.getUShort("batsmax", cfg.batMaxSoc);
  cfg.batMinSoc = prefs.getUShort("batsmin", cfg.batMinSoc);
  cfg.apcPwrLmtPct10 = prefs.getUShort("apclmt", cfg.apcPwrLmtPct10);
  cfg.apcEnabled = prefs.getBool("apcena", cfg.apcEnabled);

  cfg.displayEnabled = prefs.getBool("disp", cfg.displayEnabled);
  cfg.displayBrightness = prefs.getUChar("bright", cfg.displayBrightness);
  cfg.displayPageMs = prefs.getUInt("pageMs", cfg.displayPageMs);
  cfg.displayAutoPage = prefs.getBool("dispAuto", cfg.displayAutoPage);
  cfg.displayTimeoutS = prefs.getUShort("dispTo", cfg.displayTimeoutS);
  cfg.displayScheduleEnabled = prefs.getBool("dispSched", cfg.displayScheduleEnabled);
  // Pro Tag: aktiv (Bit 22), Beginn und Ende als Minuten seit Mitternacht (je 11 Bit).
  for (int i = 0; i < DisplaySchedule::DAYS; ++i) {
    char key[] = "dispDay0";
    key[7] += i;
    const uint32_t v = prefs.getUInt(key, UINT32_MAX);
    if (v == UINT32_MAX) continue;
    const uint16_t start = (v >> 11) & 0x7ff, end = v & 0x7ff;
    if (start < DisplaySchedule::MINUTES_PER_DAY && end < DisplaySchedule::MINUTES_PER_DAY)
      cfg.displayWeek[i] = {bool(v & (1u << 22)), start, end};
  }

  cfg.tasmotaEnabled = prefs.getBool("tasena", cfg.tasmotaEnabled);
  strlcpy(cfg.tasmotaHost, prefs.getString("tashost", cfg.tasmotaHost).c_str(), sizeof(cfg.tasmotaHost));
  cfg.tasmotaPort = prefs.getUShort("tasport", cfg.tasmotaPort);
  cfg.tasmotaPollMs = prefs.getUInt("taspoll", cfg.tasmotaPollMs);
  strlcpy(cfg.tasmotaUser, prefs.getString("tasuser", cfg.tasmotaUser).c_str(), sizeof(cfg.tasmotaUser));
  strlcpy(cfg.tasmotaPass, prefs.getString("taspass", cfg.tasmotaPass).c_str(), sizeof(cfg.tasmotaPass));
  cfg.tasmotaMeterSpoof = prefs.getBool("tasspoof", cfg.tasmotaMeterSpoof);
  cfg.tasmotaLiveGrid = prefs.getBool("taslive", cfg.tasmotaLiveGrid);

  cfg.mqttEnabled = prefs.getBool("mqena", cfg.mqttEnabled);
  strlcpy(cfg.mqttHost, prefs.getString("mqhost", cfg.mqttHost).c_str(), sizeof(cfg.mqttHost));
  cfg.mqttPort = prefs.getUShort("mqport", cfg.mqttPort);
  strlcpy(cfg.mqttUser, prefs.getString("mquser", cfg.mqttUser).c_str(), sizeof(cfg.mqttUser));
  strlcpy(cfg.mqttPass, prefs.getString("mqpass", cfg.mqttPass).c_str(), sizeof(cfg.mqttPass));
  strlcpy(cfg.mqttBase, prefs.getString("mqbase", cfg.mqttBase).c_str(), sizeof(cfg.mqttBase));
  cfg.mqttDiscovery = prefs.getBool("mqdisc", cfg.mqttDiscovery);
  cfg.mqttPublishMs = prefs.getUInt("mqpub", cfg.mqttPublishMs);
  cfg.mqttIngestEnabled = prefs.getBool("mqinena", cfg.mqttIngestEnabled);
  strlcpy(cfg.mqttIngestTopic, prefs.getString("mqintopic", cfg.mqttIngestTopic).c_str(),
          sizeof(cfg.mqttIngestTopic));
  cfg.mqttIngestMaxAgeMs = prefs.getUInt("mqinage", cfg.mqttIngestMaxAgeMs);

  cfg.cloudEnabled = prefs.getBool("cldena", cfg.cloudEnabled);
  strlcpy(cfg.cloudHost, prefs.getString("cldhost", cfg.cloudHost).c_str(), sizeof(cfg.cloudHost));
  strlcpy(cfg.cloudUser, prefs.getString("clduser", cfg.cloudUser).c_str(), sizeof(cfg.cloudUser));
  strlcpy(cfg.cloudPass, prefs.getString("cldpass", cfg.cloudPass).c_str(), sizeof(cfg.cloudPass));
  strlcpy(cfg.cloudStationId, prefs.getString("cldsid", cfg.cloudStationId).c_str(),
          sizeof(cfg.cloudStationId));
  cfg.cloudPollMs = prefs.getUInt("cldpoll", cfg.cloudPollMs);
  cfg.cloudMaxAgeMs = prefs.getUInt("clage", cfg.cloudMaxAgeMs);
  cfg.batEnabled = prefs.getBool("batena", cfg.batEnabled);
  cfg.batHitEnabled = prefs.getBool("bathit", cfg.batHitEnabled);
  cfg.batHitPollS = constrain(prefs.getUShort("bathitp", cfg.batHitPollS), 30, 3600);
  {
    const uint8_t bc = prefs.getUChar("batcld", 0);
    cfg.batCloud = static_cast<BatteryCloudType>(bc < BATTERY_CLOUD_COUNT ? bc : 0);
  }
  strlcpy(cfg.batCloudHost, prefs.getString("batchost", cfg.batCloudHost).c_str(), sizeof(cfg.batCloudHost));
  strlcpy(cfg.batCloudAppId, prefs.getString("batcid", cfg.batCloudAppId).c_str(), sizeof(cfg.batCloudAppId));
  strlcpy(cfg.batCloudSecret, prefs.getString("batcsec", cfg.batCloudSecret).c_str(), sizeof(cfg.batCloudSecret));
  strlcpy(cfg.batCloudSn, prefs.getString("batcsn", cfg.batCloudSn).c_str(), sizeof(cfg.batCloudSn));
  cfg.batCloudPollS = constrain(prefs.getUShort("batcp", cfg.batCloudPollS), BAT_CLOUD_MIN_S, 3600);

  cfg.knxEnabled = prefs.getBool("knxena", cfg.knxEnabled);
  strlcpy(cfg.knxPa, prefs.getString("knxpa", cfg.knxPa).c_str(), sizeof(cfg.knxPa));
  cfg.knxPublishMs = prefs.getUInt("knxpub", cfg.knxPublishMs);
  cfg.knxOnlyOnChange = prefs.getBool("knxchg", cfg.knxOnlyOnChange);
  cfg.knxMinDeltaW = prefs.getUShort("knxdlt", cfg.knxMinDeltaW);
  {
    String blob = prefs.getString("knxch", "");
    if (blob.length()) {
      JsonDocument d;
      if (!deserializeJson(d, blob) && d.is<JsonArray>()) {
        JsonArray a = d.as<JsonArray>();
        for (size_t i = 0; i < KNX_CH_COUNT && i < a.size(); i++) {
          cfg.knxCh[i].ena = a[i]["e"] | false;
          if (a[i]["g"].is<const char *>())
            strlcpy(cfg.knxCh[i].ga, a[i]["g"], sizeof(cfg.knxCh[i].ga));
          cfg.knxCh[i].dpt = a[i]["d"] | 0;
          // bis v0.5.6 falsche Typnummern: Strom 14.033 (ist Frequenz), Frequenz 14.035 (Wärmekapazität).
          // Gleiche 4-Byte-Kodierung, nur die angezeigte Nummer ändert sich.
          const KnxValKind kind = knxMeta(i).kind;
          if (kind == KnxValKind::Amp && cfg.knxCh[i].dpt == 14033) cfg.knxCh[i].dpt = 14019;
          if (kind == KnxValKind::Hz && cfg.knxCh[i].dpt == 14035) cfg.knxCh[i].dpt = 14033;
        }
      }
    }
  }

  cfg.uiLang = static_cast<UiLang>(prefs.getUChar("uilang", static_cast<uint8_t>(cfg.uiLang)));
  cfg.uiTheme = static_cast<UiTheme>(prefs.getUChar("uitheme", static_cast<uint8_t>(cfg.uiTheme)));

  strlcpy(cfg.deviceName, prefs.getString("name", cfg.deviceName).c_str(), sizeof(cfg.deviceName));
  return true;
}

bool save() {
  prefs.putBool("ok", true);
  prefs.putBool("eth", cfg.useEthernet);
  prefs.putBool("wifi", cfg.wifiEnabled);
  prefs.putBool("ap", cfg.wifiApMode);
  prefs.putBool("setup", cfg.setupComplete);
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);
  prefs.putString("stassid", cfg.staSsid);
  prefs.putString("stapass", cfg.staPass);
  prefs.putString("webuser", cfg.webUser);
  prefs.putString("webpass", cfg.webPass);
  prefs.putBool("httpsena", cfg.httpsEnabled);
  prefs.putBool("httpsred", cfg.httpsRedirect);
  prefs.putString("dcode", cfg.displayCode);

  prefs.putUChar("invtype", static_cast<uint8_t>(cfg.inverterType));
  prefs.putUChar("hitmode", static_cast<uint8_t>(cfg.hitMode));
  prefs.putString("hithost", cfg.hitHost);
  prefs.putUShort("hitport", cfg.hitPort);
  prefs.putUChar("hituid", cfg.hitUnitId);
  prefs.putUInt("hitpoll", cfg.hitPollMs);
  prefs.putUInt("hitbaud", cfg.hitRtuBaud);
  prefs.putBool("demo", cfg.demoMode);

  prefs.putBool("ssena", cfg.sunspecEnabled);
  prefs.putUShort("ssport", cfg.sunspecPort);
  prefs.putUChar("ssuid", cfg.sunspecUnitId);
  prefs.putBool("invpwr", cfg.invertPowerSign);
  prefs.putBool("gwena", cfg.gatewayEnabled);
  prefs.putUShort("gwport", cfg.gatewayPort);

  prefs.putBool("hwrena", cfg.hitWriteEnabled);
  prefs.putBool("ss2hit", cfg.sunspecWriteToHit);
  prefs.putUShort("batc", cfg.batMaxChargePct);
  prefs.putUShort("batd", cfg.batMaxDischargePct);
  prefs.putUShort("batsmax", cfg.batMaxSoc);
  prefs.putUShort("batsmin", cfg.batMinSoc);
  prefs.putUShort("apclmt", cfg.apcPwrLmtPct10);
  prefs.putBool("apcena", cfg.apcEnabled);

  prefs.putBool("disp", cfg.displayEnabled);
  prefs.putUChar("bright", cfg.displayBrightness);
  prefs.putUInt("pageMs", cfg.displayPageMs);
  prefs.putBool("dispAuto", cfg.displayAutoPage);
  prefs.putUShort("dispTo", cfg.displayTimeoutS);
  prefs.putBool("dispSched", cfg.displayScheduleEnabled);
  for (int i = 0; i < DisplaySchedule::DAYS; ++i) {
    char key[] = "dispDay0";
    key[7] += i;
    const auto &d = cfg.displayWeek[i];
    prefs.putUInt(key, (uint32_t(d.enabled) << 22) | (uint32_t(d.start) << 11) | d.end);
  }

  prefs.putBool("tasena", cfg.tasmotaEnabled);
  prefs.putString("tashost", cfg.tasmotaHost);
  prefs.putUShort("tasport", cfg.tasmotaPort);
  prefs.putUInt("taspoll", cfg.tasmotaPollMs);
  prefs.putString("tasuser", cfg.tasmotaUser);
  prefs.putString("taspass", cfg.tasmotaPass);
  prefs.putBool("tasspoof", cfg.tasmotaMeterSpoof);
  prefs.putBool("taslive", cfg.tasmotaLiveGrid);

  prefs.putBool("mqena", cfg.mqttEnabled);
  prefs.putString("mqhost", cfg.mqttHost);
  prefs.putUShort("mqport", cfg.mqttPort);
  prefs.putString("mquser", cfg.mqttUser);
  prefs.putString("mqpass", cfg.mqttPass);
  prefs.putString("mqbase", cfg.mqttBase);
  prefs.putBool("mqdisc", cfg.mqttDiscovery);
  prefs.putUInt("mqpub", cfg.mqttPublishMs);
  prefs.putBool("mqinena", cfg.mqttIngestEnabled);
  prefs.putString("mqintopic", cfg.mqttIngestTopic);
  prefs.putUInt("mqinage", cfg.mqttIngestMaxAgeMs);

  prefs.putBool("cldena", cfg.cloudEnabled);
  prefs.putString("cldhost", cfg.cloudHost);
  prefs.putString("clduser", cfg.cloudUser);
  prefs.putString("cldpass", cfg.cloudPass);
  prefs.putString("cldsid", cfg.cloudStationId);
  prefs.putUInt("cldpoll", cfg.cloudPollMs);
  prefs.putUInt("clage", cfg.cloudMaxAgeMs);
  prefs.putBool("batena", cfg.batEnabled);
  prefs.putBool("bathit", cfg.batHitEnabled);
  prefs.putUShort("bathitp", cfg.batHitPollS);
  prefs.putUChar("batcld", static_cast<uint8_t>(cfg.batCloud));
  prefs.putString("batchost", cfg.batCloudHost);
  prefs.putString("batcid", cfg.batCloudAppId);
  prefs.putString("batcsec", cfg.batCloudSecret);
  prefs.putString("batcsn", cfg.batCloudSn);
  prefs.putUShort("batcp", cfg.batCloudPollS);

  prefs.putBool("knxena", cfg.knxEnabled);
  prefs.putString("knxpa", cfg.knxPa);
  prefs.putUInt("knxpub", cfg.knxPublishMs);
  prefs.putBool("knxchg", cfg.knxOnlyOnChange);
  prefs.putUShort("knxdlt", cfg.knxMinDeltaW);
  {
    JsonDocument d;
    JsonArray a = d.to<JsonArray>();
    for (size_t i = 0; i < KNX_CH_COUNT; i++) {
      JsonObject o = a.add<JsonObject>();
      o["e"] = cfg.knxCh[i].ena;
      o["g"] = cfg.knxCh[i].ga;
      o["d"] = cfg.knxCh[i].dpt;
    }
    String blob;
    serializeJson(d, blob);
    prefs.putString("knxch", blob);
  }

  prefs.putUChar("uilang", static_cast<uint8_t>(cfg.uiLang));
  prefs.putUChar("uitheme", static_cast<uint8_t>(cfg.uiTheme));

  prefs.putString("name", cfg.deviceName);
  return true;
}

// Sichert den ganzen NVS-Namespace samt Typen; neue Einstellungen kommen so ohne eigene Liste mit
String exportBackup(bool withSecrets) {
  JsonDocument d;
  d["format"] = BACKUP_FORMAT;
  d["formatVersion"] = BACKUP_FORMAT_VERSION;
  d["firmware"] = PROXY_VERSION;
  d["deviceName"] = cfg.deviceName;
  d["secrets"] = withSecrets;
  JsonObject s = d["settings"].to<JsonObject>();
  nvs_iterator_t it = nullptr;
  esp_err_t r = nvs_entry_find(NVS_DEFAULT_PART_NAME, NS, NVS_TYPE_ANY, &it);
  while (r == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    const char *k = info.key;
    if (withSecrets || !isSecret(k)) {
      JsonObject e = s[String(k)].to<JsonObject>();
      switch (info.type) {
      case NVS_TYPE_U8: e["t"] = "u8"; e["v"] = prefs.getUChar(k); break;
      case NVS_TYPE_I8: e["t"] = "i8"; e["v"] = prefs.getChar(k); break;
      case NVS_TYPE_U16: e["t"] = "u16"; e["v"] = prefs.getUShort(k); break;
      case NVS_TYPE_I16: e["t"] = "i16"; e["v"] = prefs.getShort(k); break;
      case NVS_TYPE_U32: e["t"] = "u32"; e["v"] = prefs.getUInt(k); break;
      case NVS_TYPE_I32: e["t"] = "i32"; e["v"] = prefs.getInt(k); break;
      case NVS_TYPE_U64: e["t"] = "u64"; e["v"] = prefs.getULong64(k); break;
      case NVS_TYPE_I64: e["t"] = "i64"; e["v"] = prefs.getLong64(k); break;
      case NVS_TYPE_STR: e["t"] = "str"; e["v"] = prefs.getString(k); break;
      default: s.remove(String(k)); break; // Blobs nutzt die Konfiguration nicht
      }
    }
    r = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  String out;
  serializeJsonPretty(d, out);
  return out;
}

bool importBackup(const char *json, String &err) {
  const bool en = uiEnglish();
  JsonDocument d;
  if (deserializeJson(d, json)) {
    err = en ? "Invalid JSON file." : "Ungültige JSON-Datei.";
    return false;
  }
  if (strcmp(d["format"] | "", BACKUP_FORMAT) != 0) {
    err = en ? "This is not a HIT-Proxy backup." : "Keine Sicherung des HIT-Proxy.";
    return false;
  }
  if ((d["formatVersion"] | 0) != BACKUP_FORMAT_VERSION) {
    err = en ? "Unsupported backup version." : "Diese Sicherungsversion wird nicht unterstützt.";
    return false;
  }
  JsonObjectConst s = d["settings"];
  if (s.isNull() || !s["ok"].is<JsonObjectConst>()) {
    err = en ? "The backup contains no settings." : "Die Sicherung enthält keine Einstellungen.";
    return false;
  }
  // erst alles prüfen, damit eine kaputte Datei nichts halb überschreibt
  size_t entries = 0, strBytes = 0;
  for (JsonPairConst kv : s) {
    const char *k = kv.key().c_str();
    const size_t n = strlen(k);
    const EntryType t = entryType(kv.value()["t"] | "");
    if (!n || n > 15 || t == EntryType::Bad || !valueFits(t, kv.value()["v"])) {
      err = String(en ? "Invalid setting: " : "Ungültige Einstellung: ") + k;
      return false;
    }
    entries++;
    if (t == EntryType::Str) strBytes += strlen(kv.value()["v"].as<const char *>());
  }
  // die NVS-Partition ist nur 20 KB groß; eine echte Sicherung bleibt weit darunter
  if (entries > BACKUP_MAX_ENTRIES || strBytes > BACKUP_MAX_STR_TOTAL) {
    err = en ? "The backup is too large." : "Die Sicherung ist zu groß.";
    return false;
  }
  for (JsonPairConst kv : s) {
    const char *k = kv.key().c_str();
    // ohne Display ist ein Zugangscode nicht ablesbar und würde nur eine versteckte Anmeldung öffnen
    if (!HAS_DISPLAY && strcmp(k, "dcode") == 0) continue;
    if (!writeEntry(k, entryType(kv.value()["t"]), kv.value()["v"])) {
      load();
      err = String(en ? "Could not save " : "Fehler beim Speichern von ") + k +
            (en ? ". Settings were only partly restored." : ". Die Einstellungen wurden nur teilweise wiederhergestellt.");
      return false;
    }
  }
  prefs.putBool("ok", true);
  load();
  // wie enforceAccess: ohne Display gilt die Einrichtung ohne eigenes Passwort nicht als fertig
  if (!HAS_DISPLAY && !cfg.displayCode[0] && strlen(cfg.webPass) < MIN_PASS_LEN && cfg.setupComplete) {
    cfg.setupComplete = false;
    save();
  }
  return true;
}

} // namespace ConfigStore
