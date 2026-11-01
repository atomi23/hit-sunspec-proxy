#include <Arduino.h>
#include "AppConfig.h"
#include "LiveData.h"
#include "NetBoot.h"
#include "HitClient.h"
#include "SunSpecServer.h"
#include "WebConfig.h"
#include "StatusDisplay.h"
#include "TasmotaClient.h"
#include "MqttHa.h"
#include "HitControl.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nHIT-SunSpec-Proxy %s\n", PROXY_VERSION);

  ConfigStore::begin();
  auto &cfg = ConfigStore::get();
  gLive.setupScreen = !cfg.setupComplete;

  NetBoot::begin(cfg);
  StatusDisplay::begin(cfg);
  HitClient::begin(cfg);
  HitControl::begin(cfg);
  TasmotaClient::begin(cfg);
  SunSpecServer::begin(cfg);
  MqttHa::begin(cfg);
  WebConfig::begin();

  HitClient::pollNow();
  TasmotaClient::pollNow();
  SunSpecServer::rebuildMap();
  MqttHa::publishNow();

  Serial.printf("Web-UI: http://%s/  User %s\n", gLive.ipShown, cfg.webUser);
  Serial.printf("Display-Code: %s  Web-Pass: %s\n",
                cfg.displayCode, cfg.webPass[0] ? "(gesetzt)" : "(leer → Code nutzen)");
  if (!cfg.setupComplete) {
    Serial.printf("SETUP AP: SSID \"%s\"\n", cfg.wifiSsid);
  }
  if (cfg.demoMode) Serial.println("*** DEMO MODE — kein HIT ***");
  if (cfg.hitWriteEnabled) Serial.println("HIT Write: AN");
  Serial.printf("SunSpec: %s:%u\n", gLive.ipShown, cfg.sunspecPort);
  Serial.printf("HIT Ziel: %s:%u mode=%d\n", cfg.hitHost, cfg.hitPort, int(cfg.hitMode));
  if (cfg.tasmotaEnabled) Serial.printf("Tasmota: %s spoof=%d\n", cfg.tasmotaHost, int(cfg.tasmotaMeterSpoof));
  if (cfg.mqttEnabled) Serial.printf("MQTT: %s:%u base=%s\n", cfg.mqttHost, cfg.mqttPort, cfg.mqttBase);
}

void loop() {
  auto &cfg = ConfigStore::get();
  NetBoot::loop(cfg);
  HitClient::loop();
  TasmotaClient::loop();
  SunSpecServer::loop();
  MqttHa::loop();
  StatusDisplay::loop();
  WebConfig::loop();
  delay(2);
}
