#pragma once
#include <Arduino.h>

// Selbsttest der Weboberfläche. Modbus/SunSpec läuft in der Hauptschleife weiter, auch wenn der
// Webserver (AsyncTCP-Task) oder der HTTPS-Proxy (httpd-Task) hängt; der Task-Watchdog merkt das nicht.
namespace WebHealth {
enum class Reason : uint8_t { None = 0, Http = 1, Https = 2 };

void begin();               // nach WebConfig::begin(); fragt Port 80 über 127.0.0.1 ab (eigene Task)
void restart(Reason why);   // geordneter Neustart; der Grund steht nach dem Start im Systemprotokoll
}
