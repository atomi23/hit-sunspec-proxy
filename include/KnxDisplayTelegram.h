#pragma once
#include <stddef.h>
#include <stdint.h>

namespace KnxDisplayTelegram {
constexpr size_t MAX_SIZE = 6 + 2 + 255 + 9 + 3; // KNXnet/IP, cEMI, Zusatzinfo, L_Data

inline uint16_t readWord(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }

// Verbindungsloses GroupValueWrite von einem anderen Gerät. n = 0: Wert bis 6 Bit in value,
// sonst n Datenbytes ab data.
inline bool readWrite(const uint8_t *p, size_t len, uint16_t ownPa, uint16_t &ga, uint8_t &value,
                      const uint8_t *&data, uint8_t &n) {
  if (len < 17 || p[0] != 6 || p[1] != 0x10 || readWord(p + 2) != 0x0530 ||
      readWord(p + 4) != len || p[6] != 0x29) return false;
  const size_t offset = 8 + p[7]; // cEMI Additional Information Length
  if (offset + 9 > len) return false;
  const uint8_t *frame = p + offset;
  if (frame[6] < 1 || offset + 8 + frame[6] != len) return false;
  if (!(frame[1] & 0x80) || readWord(frame + 2) == ownPa) return false;
  if (frame[7] != 0 || (frame[8] & 0xc0) != 0x80) return false;
  n = uint8_t(frame[6] - 1);
  if (n && frame[8] != 0x80) return false;
  ga = readWord(frame + 4);
  value = frame[8] & 0x3f;
  data = frame + 9;
  return true;
}

// DPT 5.001 (0..255) auf Prozent.
inline uint8_t scalingPercent(uint8_t raw) { return uint8_t((raw * 100 + 127) / 255); }

inline bool matchesDisplaySwitch(uint16_t ga, uint16_t displayGa, uint8_t value, uint8_t n) {
  return ga == displayGa && n == 0 && value <= 1;
}
}
