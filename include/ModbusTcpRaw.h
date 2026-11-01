#pragma once
#include <Arduino.h>

namespace ModbusTcpRaw {
bool readInputRegs(const char *host, uint16_t port, uint8_t unit,
                   uint16_t addr, uint16_t qty, uint16_t *out,
                   uint32_t timeoutMs = 1500);

bool readHoldingRegs(const char *host, uint16_t port, uint8_t unit,
                     uint16_t addr, uint16_t qty, uint16_t *out,
                     uint32_t timeoutMs = 1500);

bool writeHoldingReg(const char *host, uint16_t port, uint8_t unit,
                     uint16_t addr, uint16_t value,
                     uint32_t timeoutMs = 1500);
}
