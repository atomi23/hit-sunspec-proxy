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
#include "HitBus.h"
#include "ModbusGateway.h"
#include "History.h"
#include "EventLog.h"
#include "HttpsServer.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nHIT-SunSpec-Proxy %s\n", PROXY_VERSION);

  ConfigStore::begin();
  auto &cfg = ConfigStore::get();
  gLive.setupScreen = !cfg.setupComplete;

  Watchdog::begin(45);
  Serial.printf("Letzter Neustart: %s\n", Watchdog::lastResetReason());
  EventLog::add('I', "System", "Start v%s, Grund: %s", PROXY_VERSION, Watchdog::lastResetReason());

  NetBoot::begin(cfg);
  StatusDisplay::begin(cfg);
  HitBus::begin(cfg);
  HitClient::begin(cfg);
  ModbusGateway::begin(cfg);
  HitControl::begin(cfg);
  TasmotaClient::begin(cfg);
  SunSpecServer::begin(cfg);
  MqttHa::begin(cfg);
  HoyCloud::begin(cfg);
  KnxIp::begin(cfg);
  DataBroker::begin();
  WebConfig::begin();
  History::begin();
  HttpsServer::begin(cfg); // nach History: braucht das Dateisystem

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
  Serial.printf("Display: Seitenwechsel %lus, aus nach %us\n", (unsigned long)(cfg.displayPageMs / 1000),
                cfg.displayTimeoutS);
  if (cfg.hitWriteEnabled) Serial.println("HIT Write: AN");
  Serial.printf("SunSpec: %s:%u\n", gLive.ipShown, cfg.sunspecPort);
  if (cfg.hitMode == HitLinkMode::RtuRs485)
    Serial.printf("HIT: RS485 %lu Baud, Unit %u\n", (unsigned long)cfg.hitRtuBaud, cfg.hitUnitId);
  else
    Serial.printf("HIT: %s:%u Unit %u\n", cfg.hitHost, cfg.hitPort, cfg.hitUnitId);
  if (cfg.gatewayEnabled) Serial.printf("Modbus-Gateway: Port %u\n", cfg.gatewayPort);
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
  History::loop();
  KnxIp::loop();
  SunSpecServer::loop();
  ModbusGateway::loop();
  StatusDisplay::loop();
  WebConfig::loop();
  Watchdog::checkSoft();
  Watchdog::feed();
  delay(2);
}
