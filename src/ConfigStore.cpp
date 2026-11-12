#include "AppConfig.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_system.h>

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
  randomPass(cfg.displayCode, sizeof(cfg.displayCode));
  strlcpy(cfg.wifiPass, cfg.displayCode, sizeof(cfg.wifiPass)); // AP = Display-Code
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
    Serial.printf("First-Boot: AP \"%s\" Display-Code \"%s\" (Web-Pass leer)\n",
                  cfg.wifiSsid, cfg.displayCode);
  } else {
    load();
    if (!cfg.displayCode[0]) {
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

  cfg.hitMode = static_cast<HitLinkMode>(prefs.getUChar("hitmode", static_cast<uint8_t>(cfg.hitMode)));
  strlcpy(cfg.hitHost, prefs.getString("hithost", cfg.hitHost).c_str(), sizeof(cfg.hitHost));
  cfg.hitPort = prefs.getUShort("hitport", cfg.hitPort);
  cfg.hitUnitId = prefs.getUChar("hituid", cfg.hitUnitId);
  cfg.hitPollMs = prefs.getUInt("hitpoll", cfg.hitPollMs);
  cfg.hitRtuBaud = prefs.getUInt("hitbaud", cfg.hitRtuBaud);
  cfg.demoMode = prefs.getBool("demo", cfg.demoMode);

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

  cfg.tasmotaEnabled = prefs.getBool("tasena", cfg.tasmotaEnabled);
  strlcpy(cfg.tasmotaHost, prefs.getString("tashost", cfg.tasmotaHost).c_str(), sizeof(cfg.tasmotaHost));
  cfg.tasmotaPort = prefs.getUShort("tasport", cfg.tasmotaPort);
  cfg.tasmotaPollMs = prefs.getUInt("taspoll", cfg.tasmotaPollMs);
  strlcpy(cfg.tasmotaUser, prefs.getString("tasuser", cfg.tasmotaUser).c_str(), sizeof(cfg.tasmotaUser));
  strlcpy(cfg.tasmotaPass, prefs.getString("taspass", cfg.tasmotaPass).c_str(), sizeof(cfg.tasmotaPass));
  cfg.tasmotaMeterSpoof = prefs.getBool("tasspoof", cfg.tasmotaMeterSpoof);

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

  prefs.putUChar("hitmode", static_cast<uint8_t>(cfg.hitMode));
  prefs.putString("hithost", cfg.hitHost);
  prefs.putUShort("hitport", cfg.hitPort);
  prefs.putUChar("hituid", cfg.hitUnitId);
  prefs.putUInt("hitpoll", cfg.hitPollMs);
  prefs.putUInt("hitbaud", cfg.hitRtuBaud);
  prefs.putBool("demo", cfg.demoMode);

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

  prefs.putBool("tasena", cfg.tasmotaEnabled);
  prefs.putString("tashost", cfg.tasmotaHost);
  prefs.putUShort("tasport", cfg.tasmotaPort);
  prefs.putUInt("taspoll", cfg.tasmotaPollMs);
  prefs.putString("tasuser", cfg.tasmotaUser);
  prefs.putString("taspass", cfg.tasmotaPass);
  prefs.putBool("tasspoof", cfg.tasmotaMeterSpoof);

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

} // namespace ConfigStore
