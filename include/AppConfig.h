#pragma once
#include <Arduino.h>
#include "DisplaySchedule.h"
#include "KnxPoints.h"
#include "UiPrefs.h"

enum class HitLinkMode : uint8_t {
  Tcp = 0,
  RtuRs485 = 1,
};

// Wechselrichter-Treiber; bisher nur der HIT, weitere kommen als eigene Werte dazu
enum class InverterType : uint8_t {
  HoymilesHit = 0,
};
constexpr uint8_t INVERTER_TYPE_COUNT = 1;

// Akku-Cloud-Anbieter; weitere kommen als eigene Werte dazu
enum class BatteryCloudType : uint8_t {
  None = 0,
  Dyness = 1,
};
constexpr uint8_t BATTERY_CLOUD_COUNT = 2;
// Dyness erlaubt rund einen Abruf je 5 min; ohne PSRAM seltener, die TLS-Sitzung braucht viel internen RAM
#ifdef BOARD_HAS_PSRAM
constexpr uint16_t BAT_CLOUD_MIN_S = 300;
#else
constexpr uint16_t BAT_CLOUD_MIN_S = 600;
#endif

// Zugang: mit Display ein zufälliger Code (steht dort samt QR-Code). Ohne Display kann niemand einen Code
// ablesen; dort gilt bis zur Einrichtung ein festes Passwort, danach nur noch das selbst gewählte.
#ifdef BOARD_T_CONNECT_PRO
constexpr bool HAS_DISPLAY = true;
#else
constexpr bool HAS_DISPLAY = false;
#endif
constexpr char DEFAULT_PASS[] = "hitproxy1";
constexpr size_t MIN_PASS_LEN = 8; // WPA2 verlangt mindestens 8 Zeichen

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
  bool httpsEnabled = true;    // Port 443 vor der Weboberfläche
  bool httpsRedirect = true;   // HTTP-Aufrufe von außen auf HTTPS umleiten

  // HIT source
  InverterType inverterType = InverterType::HoymilesHit;
  HitLinkMode hitMode = HitLinkMode::Tcp;
  char hitHost[64] = "192.0.2.50"; // Beispiel-IP (RFC 5737), keine echte Anlage
  uint16_t hitPort = 502;
  uint8_t hitUnitId = 1;
  uint32_t hitPollMs = 30000;
  uint32_t hitRtuBaud = 9600;
  // Demo: Fake-Live-Daten, kein HIT-Poll, keine Writes an HIT (Web/Display/SunSpec lokal testen)
  bool demoMode = false;

  // SunSpec / Consolinno
  bool sunspecEnabled = true;  // Hauptfunktion; abschaltbar, wenn nur Gateway/HA/KNX gebraucht wird
  uint16_t sunspecPort = 1502; // wie früher Node-RED; 502 bleibt fürs HIT-Gateway
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
  bool displayAutoPage = false;   // aus: Live-Seite bleibt stehen, Antippen blättert
  uint32_t displayPageMs = 8000;
  uint16_t displayTimeoutS = 300; // Hintergrundlicht aus nach Inaktivität, 0 = immer an
  bool displayScheduleEnabled = false;
  DisplayDay displayWeek[DisplaySchedule::DAYS] = {};

  // Tasmota
  bool tasmotaEnabled = false;
  char tasmotaHost[64] = "192.0.2.60"; // Beispiel-IP
  uint16_t tasmotaPort = 80;
  uint32_t tasmotaPollMs = 5000;
  char tasmotaUser[32] = "";
  char tasmotaPass[64] = "";
  bool tasmotaMeterSpoof = false;
  bool tasmotaLiveGrid = false; // Live-View/Display: Netz vom Tasmota statt HIT

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

  // Akkudaten: zusätzliche Werte vom HIT und optional aus der Cloud des Akku-Herstellers
  bool batEnabled = true;
  bool batHitEnabled = true;
  uint16_t batHitPollS = 60;     // 30..3600
  BatteryCloudType batCloud = BatteryCloudType::None;
  char batCloudHost[64] = "open-api.dyness.com";
  char batCloudAppId[64] = "";
  char batCloudSecret[80] = ""; // wird nie ausgeliefert
  char batCloudSn[40] = "";     // leer = erstes Gerät des Kontos
  uint16_t batCloudPollS = BAT_CLOUD_MIN_S;

  // KNX IP Routing — alle Live-Werte wählbar (siehe KnxPoints.h)
  bool knxEnabled = false;
  char knxPa[16] = "15.15.250";
  uint32_t knxPublishMs = 10000;
  bool knxOnlyOnChange = true;
  uint16_t knxMinDeltaW = 50;
  KnxCh knxCh[KNX_CH_COUNT] = {};

  // Bluetooth-LED-Strip (App "Lotus Lantern"), geschaltet über KNX
  bool bleLedEnabled = false;
  char bleLedAddr[18] = "";     // MAC aa:bb:cc:dd:ee:ff
  uint8_t bleLedAddrType = 0;   // 0 = public, 1 = random
  char bleLedName[24] = "";     // Bluetooth-Name, bestimmt den Befehlssatz
  uint8_t bleLedVariant = 0;    // LotusLantern::Variant, 0 = automatisch nach Name

  // UI: Sprache + Farbschema (Web + Display)
  UiLang uiLang = UiLang::De;
  UiTheme uiTheme = UiTheme::Mint;

  char deviceName[32] = "HIT-SunSpec-Proxy";
};

namespace ConfigStore {
  void begin();
  AppConfig &get();
  inline bool uiEnglish() { return get().uiLang == UiLang::En; }
  inline const char *uiText(const char *de, const char *en) { return uiEnglish() ? en : de; }
  bool load();
  bool save();
  void resetDefaults();
  void provisionFirstBoot();
  // Löscht alle gespeicherten Einstellungen; danach neu starten → Erststart mit neuem Display-Code
  void factoryReset();
  // Neuer zufälliger Zugangscode (Login + Passwort des Geräte-WLANs), sofort gespeichert
  const char *newDisplayCode();
  // Sicherung als JSON; withSecrets=false lässt Passwörter, Schlüssel und Zugangscode weg
  constexpr char BACKUP_FORMAT[] = "hit-sunspec-proxy-config";
  constexpr int BACKUP_FORMAT_VERSION = 1;
constexpr size_t BACKUP_MAX_ENTRIES = 256;
constexpr size_t BACKUP_MAX_STR = 2048;
constexpr size_t BACKUP_MAX_STR_TOTAL = 6144;
  String exportBackup(bool withSecrets);
  // Prüft die ganze Datei, schreibt dann die enthaltenen Einträge; fehlende bleiben unverändert. Danach neu starten.
  bool importBackup(const char *json, String &err);
}
