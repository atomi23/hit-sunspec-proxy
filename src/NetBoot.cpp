#include "AppConfig.h"
#include "LiveData.h"
#include <WiFi.h>
#include <esp_log.h>

#if defined(BOARD_T_CONNECT_PRO)
#include <ETH.h>
// W5500 SPI Ethernet on T-Connect-Pro (pins laut LilyGO Wiki)
#ifndef ETH_PHY_TYPE
#define ETH_PHY_TYPE ETH_PHY_W5500
#endif
#endif

#if defined(BOARD_T_POE_PRO)
#include <ETH.h>
#endif

namespace NetBoot {

static AppConfig pending;
static volatile bool wifiDirty = false;
static char activeSta[33] = "";
static char activeStaPass[65] = "";
static char activeApPass[65] = "";
static bool activeAp = false;
static bool activeWifi = false;

static void refreshIps(const AppConfig &cfg) {
  strlcpy(gLive.ipEth, "-", sizeof(gLive.ipEth));
  strlcpy(gLive.ipWifi, "-", sizeof(gLive.ipWifi));
  strlcpy(gLive.ipShown, "-", sizeof(gLive.ipShown));

#if defined(ETH_PHY_TYPE) || defined(BOARD_T_CONNECT_PRO) || defined(BOARD_T_POE_PRO)
  if (cfg.useEthernet && ETH.linkUp()) {
    gLive.ethOk = true;
    strlcpy(gLive.ipEth, ETH.localIP().toString().c_str(), sizeof(gLive.ipEth));
  } else {
    gLive.ethOk = false;
  }
#else
  gLive.ethOk = false;
#endif

  gLive.wifiOk = false;
  const wifi_mode_t mode = WiFi.getMode();
  if ((mode == WIFI_STA || mode == WIFI_AP_STA) && WiFi.status() == WL_CONNECTED) {
    gLive.wifiOk = true;
    strlcpy(gLive.ipWifi, WiFi.localIP().toString().c_str(), sizeof(gLive.ipWifi));
  } else if (mode == WIFI_AP || mode == WIFI_AP_STA) {
    gLive.wifiOk = true;
    strlcpy(gLive.ipWifi, WiFi.softAPIP().toString().c_str(), sizeof(gLive.ipWifi));
  }

  // Ethernet zuerst (Consolinno), dann Heim-WLAN, zuletzt Geräte-WLAN
  if (gLive.ethOk && strcmp(gLive.ipEth, "-") != 0) {
    strlcpy(gLive.ipShown, gLive.ipEth, sizeof(gLive.ipShown));
  } else if (gLive.wifiOk && strcmp(gLive.ipWifi, "-") != 0) {
    strlcpy(gLive.ipShown, gLive.ipWifi, sizeof(gLive.ipShown));
  }
}

static void startWifi(const AppConfig &cfg) {
  if (WiFi.getMode() != WIFI_OFF) { // beim ersten Start ist WLAN noch nicht initialisiert
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
  }
  activeWifi = cfg.wifiEnabled;
  strlcpy(activeSta, cfg.staSsid, sizeof(activeSta));
  strlcpy(activeStaPass, cfg.staPass, sizeof(activeStaPass));
  strlcpy(activeApPass, cfg.wifiPass, sizeof(activeApPass));
  if (!cfg.wifiEnabled) {
    WiFi.mode(WIFI_OFF);
    activeAp = false;
    return;
  }
  const bool sta = cfg.staSsid[0] != 0;
  // ohne Heim-WLAN immer Geräte-WLAN, sonst wäre das Gerät ohne Kabel nicht erreichbar
  activeAp = cfg.wifiApMode || !sta;
  WiFi.mode(activeAp && sta ? WIFI_AP_STA : (sta ? WIFI_STA : WIFI_AP));
  if (activeAp) WiFi.softAP(cfg.wifiSsid, cfg.wifiPass);
  if (sta) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(cfg.staSsid, cfg.staPass);
  }
}

void begin(const AppConfig &cfg) {
  startWifi(cfg);

#if defined(BOARD_T_CONNECT_PRO)
  if (cfg.useEthernet) {
    // W5500 über Arduino-SPI, wie LovyanGFX; beide sperren über denselben SPIClass-Bus.
    // (Mit dem IDF-SPI-Treiber startet die PHY auf diesem Board nicht.)
    SPI.begin(12, 13, 11); // SCLK, MISO, MOSI; ist schon offen, wenn das Display vorher gestartet hat
    ETH.begin(ETH_PHY_W5500, 1, 10, 9, 48, SPI);
    // Fehlermeldungen des Treibers kommen bei Empfangsproblemen in einer engen Schleife und hungern
    // über die UART-Ausgabe die Hauptschleife aus (Task-Watchdog); Pakete gehen dann ohnehin verloren
    esp_log_level_set("w5500.mac", ESP_LOG_NONE);
  }
#elif defined(BOARD_T_POE_PRO)
  if (cfg.useEthernet) {
    ETH.begin();
  }
#endif

  delay(500);
  refreshIps(cfg);
}

// aus dem Web-Task: WLAN erst im loop() neu aufbauen, und nur wenn sich daran etwas geändert hat
void applyConfig(const AppConfig &cfg) {
  const bool apWanted = cfg.wifiApMode || !cfg.staSsid[0];
  if (cfg.wifiEnabled == activeWifi && apWanted == activeAp && strcmp(cfg.staSsid, activeSta) == 0 &&
      strcmp(cfg.staPass, activeStaPass) == 0 && strcmp(cfg.wifiPass, activeApPass) == 0)
    return;
  pending = cfg;
  wifiDirty = true;
}

void loop(const AppConfig &cfg) {
  if (wifiDirty) {
    wifiDirty = false;
    delay(300); // Antwort auf das Speichern noch ausliefern
    startWifi(pending);
  }
  static uint32_t next = 0;
  if (millis() < next) return;
  next = millis() + 2000;
  refreshIps(cfg);
}

} // namespace NetBoot
