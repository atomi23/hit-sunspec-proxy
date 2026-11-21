#pragma once
#include <stddef.h>
#include <stdint.h>

namespace KnxDisplayTelegram {
constexpr size_t MAX_SIZE = 6 + 2 + 255 + 9; // KNXnet/IP, cEMI, Zusatzinfo, L_Data

inline uint16_t readWord(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }

inline bool readSwitch(const uint8_t *p, size_t len, uint16_t ga, uint16_t ownPa, bool &on) {
  if (len < 17 || p[0] != 6 || p[1] != 0x10 || readWord(p + 2) != 0x0530 ||
      readWord(p + 4) != len || p[6] != 0x29) return false;
  const size_t offset = 8 + p[7]; // cEMI Additional Information Length
  if (offset + 9 != len) return false;
  const uint8_t *frame = p + offset;
  if (!(frame[1] & 0x80) || readWord(frame + 2) == ownPa || readWord(frame + 4) != ga) return false;
  // Nur verbindungslose GroupValueWrite-Telegramme mit einem Bit (DPT 1.001).
  if (frame[6] != 1 || frame[7] != 0 || (frame[8] & 0xfe) != 0x80) return false;
  on = frame[8] & 1;
  return true;
}
}
