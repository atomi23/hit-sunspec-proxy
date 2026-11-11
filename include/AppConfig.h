#pragma once
#include <Arduino.h>
#include "KnxPoints.h"
#include "UiPrefs.h"

enum class HitLinkMode : uint8_t {
  Tcp = 0,
  RtuRs485 = 1,
};

struct AppConfig {
  // Network
  bool useEthernet = true;
  bool wifiEnabled = true;
  char wifiSsid[33] = "HIT-Proxy";
  char wifiPass[65] = "hitproxy1"; // = Display-Code (AP)
  bool wifiApMode = true;
  bool setupComplete = false;
  // Heim-WLAN (Client); das Geräte-WLAN oben bleibt davon getrennt
  char staSsid[33] = "";
  char staPass[65] = "";

  // Auth: Display-Code (random, immer auf Display) + selbst gewähltes Web-Passwort
  char webUser[32] = "admin";
  char displayCode[17] = "";   // random, persistent, Login-Fallback
  char webPass[65] = "";       // selbst gewählt; leer = nur Display-Code

  // HIT source
  HitLinkMode hitMode = HitLinkMode::Tcp;
  char hitHost[64] = "192.0.2.50"; // Beispiel-IP (RFC 5737), keine echte Anlage
  uint16_t hitPort = 502;
  uint8_t hitUnitId = 1;
  uint32_t hitPollMs = 25000;
  uint32_t hitRtuBaud = 9600;
  // Demo: Fake-Live-Daten, kein HIT-Poll, keine Writes an HIT (Web/Display/SunSpec lokal testen)
  bool demoMode = false;

  // SunSpec / Consolinno
  uint16_t sunspecPort = 1502;
  uint8_t sunspecUnitId = 1;
  bool invertPowerSign = false;

  // Modbus-Gateway: andere Clients (z.B. Home Assistant) lesen den HIT über den ESP
  bool gatewayEnabled = false;
  uint16_t gatewayPort = 502;

  // HIT Schreib-Steuerung (optional, default AUS)
  bool hitWriteEnabled = false;
  bool sunspecWriteToHit = false; // Consolinno Model-123 → HIT APC/Akku
  uint16_t batMaxChargePct = 100;    // Reg 306 raw 10..100
  uint16_t batMaxDischargePct = 100; // Reg 307
  uint16_t batMaxSoc = 100;          // Reg 308
  uint16_t batMinSoc = 10;           // Reg 309
  uint16_t apcPwrLmtPct10 = 1000;    // Reg 280, %*10 → 1000=100%
  bool apcEnabled = false;           // Reg 279

  // Display
  bool displayEnabled = true;
  uint8_t displayBrightness = 200;
  uint32_t displayPageMs = 8000;
  uint16_t displayTimeoutS = 300; // Hintergrundlicht aus nach Inaktivität, 0 = immer an

  // Tasmota
  bool tasmotaEnabled = false;
  char tasmotaHost[64] = "192.0.2.60"; // Beispiel-IP
  uint16_t tasmotaPort = 80;
  uint32_t tasmotaPollMs = 5000;
  char tasmotaUser[32] = "";
  char tasmotaPass[64] = "";
  bool tasmotaMeterSpoof = false;

  // MQTT (Publish → HA + optional Ingest-Fallback)
  bool mqttEnabled = false;
  char mqttHost[64] = "192.0.2.10"; // Beispiel-IP
  uint16_t mqttPort = 1883;
  char mqttUser[32] = "";
  char mqttPass[64] = "";
  char mqttBase[48] = "hitproxy";
  bool mqttDiscovery = true;
  uint32_t mqttPublishMs = 5000;
  bool mqttIngestEnabled = false;
  char mqttIngestTopic[80] = "hitproxy/in"; // JSON: pv_w,bat_w,grid_w,soc,…
  uint32_t mqttIngestMaxAgeMs = 120000;

  // Hoymiles Cloud Fallback (wenn HIT tot)
  bool cloudEnabled = false;
  char cloudHost[64] = "neapi.hoymiles.com";
  char cloudUser[64] = "";
  char cloudPass[64] = "";
  char cloudStationId[24] = ""; // sid / station_id
  uint32_t cloudPollMs = 60000;
  uint32_t cloudMaxAgeMs = 180000;

  // KNX IP Routing — alle Live-Werte wählbar (siehe KnxPoints.h)
  bool knxEnabled = false;
  char knxPa[16] = "15.15.250";
  uint32_t knxPublishMs = 10000;
  bool knxOnlyOnChange = true;
  uint16_t knxMinDeltaW = 50;
  KnxCh knxCh[KNX_CH_COUNT] = {};

  // UI: Sprache + Farbschema (Web + Display)
  UiLang uiLang = UiLang::De;
  UiTheme uiTheme = UiTheme::Mint;

  char deviceName[32] = "HIT-SunSpec-Proxy";
};

namespace ConfigStore {
  void begin();
  AppConfig &get();
  bool load();
  bool save();
  void resetDefaults();
  void provisionFirstBoot();
  // Löscht alle gespeicherten Einstellungen; danach neu starten → Erststart mit neuem Display-Code
  void factoryReset();
}
