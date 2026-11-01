#include "AppConfig.h"
#include "LiveData.h"
#include <WiFi.h>

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

  if (WiFi.getMode() != WIFI_OFF) {
    if (cfg.wifiApMode) {
      gLive.wifiOk = true;
      strlcpy(gLive.ipWifi, WiFi.softAPIP().toString().c_str(), sizeof(gLive.ipWifi));
    } else if (WiFi.status() == WL_CONNECTED) {
      gLive.wifiOk = true;
      strlcpy(gLive.ipWifi, WiFi.localIP().toString().c_str(), sizeof(gLive.ipWifi));
    } else {
      gLive.wifiOk = false;
    }
  }

  // Prefer Ethernet IP on display (Consolinno bind target)
  if (gLive.ethOk && strcmp(gLive.ipEth, "-") != 0) {
    strlcpy(gLive.ipShown, gLive.ipEth, sizeof(gLive.ipShown));
  } else if (gLive.wifiOk && strcmp(gLive.ipWifi, "-") != 0) {
    strlcpy(gLive.ipShown, gLive.ipWifi, sizeof(gLive.ipShown));
  }
}

void begin(const AppConfig &cfg) {
  if (cfg.wifiEnabled) {
    if (cfg.wifiApMode) {
      WiFi.mode(WIFI_AP);
      WiFi.softAP(cfg.wifiSsid, cfg.wifiPass);
    } else {
      WiFi.mode(WIFI_STA);
      WiFi.begin(cfg.wifiSsid, cfg.wifiPass);
    }
  }

#if defined(BOARD_T_CONNECT_PRO)
  if (cfg.useEthernet) {
    // SPI W5500 — Pinning laut LilyGO T-Connect-Pro
    SPI.begin(12, 13, 11); // SCLK, MISO, MOSI shared with display bus
    ETH.begin(ETH_PHY_W5500, 1, 10, 9, 48, SPI);
  }
#elif defined(BOARD_T_POE_PRO)
  if (cfg.useEthernet) {
    ETH.begin();
  }
#endif

  delay(500);
  refreshIps(cfg);
}

void loop(const AppConfig &cfg) {
  static uint32_t next = 0;
  if (millis() < next) return;
  next = millis() + 2000;
  refreshIps(cfg);
}

} // namespace NetBoot
