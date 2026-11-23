// Minimaler Arduino-Ersatz, damit Firmware-Module auf dem PC laufen (siehe tools/hitbus_rtu_test.cpp)
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <freertos/semphr.h>

using std::max;
using std::min;

inline size_t strlcpy(char *dst, const char *src, size_t n) {
  const size_t len = strlen(src);
  if (n) {
    const size_t c = len < n - 1 ? len : n - 1;
    memcpy(dst, src, c);
    dst[c] = 0;
  }
  return len;
}

using String = std::string;

template <typename T> T constrain(T x, T low, T high) {
  return std::clamp(x, low, high);
}

inline uint32_t millis() { return 0; }

inline uint32_t hostMillis;
inline uint32_t millis() { return hostMillis; }

inline uint32_t micros() {
  static uint32_t t = 0;
  return t += 100;
}
inline void delayMicroseconds(uint32_t) {}

constexpr uint32_t SERIAL_8N1 = 0;

// Nach jedem write() liegt die vom Test vorgegebene Antwort im Empfang; danach wie ein Timeout
struct HardwareSerial {
  std::deque<uint8_t> reply, rx;
  void begin(uint32_t, uint32_t, int, int) {}
  void end() {}
  void setTimeout(uint32_t) {}
  int available() { return int(rx.size()); }
  int read() {
    if (rx.empty()) return -1;
    const int b = rx.front();
    rx.pop_front();
    return b;
  }
  size_t write(const uint8_t *, size_t n) {
    rx = reply;
    return n;
  }
  void flush() {}
  size_t readBytes(uint8_t *buf, size_t n) {
    size_t got = 0;
    while (got < n && !rx.empty()) buf[got++] = uint8_t(read());
    return got;
  }
};
inline HardwareSerial Serial1;
