#pragma once
#include "AppConfig.h"

// Modbus-TCP-Gateway zum HIT: ersetzt den Modbus-Proxy in Home Assistant. Lesen immer,
// Schreiben nur mit „Schreiben an HIT erlauben“.
namespace ModbusGateway {
void begin(const AppConfig &cfg);
void applyConfig(const AppConfig &cfg);
void loop();
}
