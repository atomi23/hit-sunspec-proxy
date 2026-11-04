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
#include "HoyCloud.h"
#include "KnxIp.h"
#include "DataSource.h"
#include "Watchdog.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nHIT-SunSpec-Proxy %s\n", PROXY_VERSION);

  ConfigStore::begin();
  auto &cfg = ConfigStore::get();
  gLive.setupScreen = !cfg.setupComplete;

  Watchdog::begin(45);

  NetBoot::begin(cfg);
  StatusDisplay::begin(cfg);
  HitClient::begin(cfg);
  HitControl::begin(cfg);
  TasmotaClient::begin(cfg);
  SunSpecServer::begin(cfg);
  MqttHa::begin(cfg);
  HoyCloud::begin(cfg);
  KnxIp::begin(cfg);
  DataBroker::begin();
  WebConfig::begin();

  HitClient::pollNow();
  TasmotaClient::pollNow();
  DataBroker::reconcile();
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
  if (cfg.mqttEnabled || cfg.mqttIngestEnabled)
    Serial.printf("MQTT: %s:%u pub=%d ingest=%d topic=%s\n", cfg.mqttHost, cfg.mqttPort,
                  int(cfg.mqttEnabled), int(cfg.mqttIngestEnabled), cfg.mqttIngestTopic);
  if (cfg.cloudEnabled) Serial.printf("Cloud: %s sid=%s\n", cfg.cloudHost, cfg.cloudStationId);
  if (cfg.knxEnabled) Serial.printf("KNX IP: PA %s every %lums\n", cfg.knxPa, (unsigned long)cfg.knxPublishMs);
}

void loop() {
  Watchdog::feed();
  auto &cfg = ConfigStore::get();
  NetBoot::loop(cfg);
  HitClient::loop();
  TasmotaClient::loop();
  MqttHa::loop();
  HoyCloud::loop();
  DataBroker::reconcile();
  KnxIp::loop();
  SunSpecServer::loop();
  StatusDisplay::loop();
  WebConfig::loop();
  Watchdog::checkSoft();
  Watchdog::feed();
  delay(2);
}
