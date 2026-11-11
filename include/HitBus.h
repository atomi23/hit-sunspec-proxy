#pragma once
#include <Arduino.h>
#include "AppConfig.h"

// Die eine Leitung zum HIT (Modbus TCP oder RTU über RS485). Der HIT erlaubt nur eine aktive
// Verbindung, deshalb laufen Poll, Steuerung und Gateway alle hierüber, gegenseitig gesperrt.
namespace HitBus {
void begin(const AppConfig &cfg);
void applyConfig(const AppConfig &cfg);

// Roh-PDU (Funktionscode + Daten) senden, Antwort-PDU zurück. false bei Zeitüberschreitung oder
// Verbindungsfehler; eine Modbus-Exception ist eine gültige Antwort (resp[0] & 0x80).
bool transact(uint8_t unit, const uint8_t *pdu, size_t pduLen, uint8_t *resp, size_t respMax,
              size_t &respLen);

// Hoymiles V2.8: FC6 auf 101–299 setzt alle Netzschutz-Parameter auf Werkseinstellung.
// Solche Anfragen werden nie gesendet; Werte dort nur per FC16 schreiben.
constexpr uint16_t SAFETY_FIRST = 101;
constexpr uint16_t SAFETY_LAST = 299;
bool forbidden(const uint8_t *pdu, size_t pduLen);

bool readInput(uint16_t addr, uint16_t qty, uint16_t *out);
bool readHolding(uint16_t addr, uint16_t qty, uint16_t *out);
bool writeSingle(uint16_t addr, uint16_t value);
bool writeMulti(uint16_t addr, uint16_t qty, const uint16_t *values);

void close();
}
