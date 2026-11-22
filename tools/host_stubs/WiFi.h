#pragma once
#include <Arduino.h>

struct WiFiClient {
  bool connected() { return false; }
  void stop() {}
  bool connect(const char *, uint16_t, int32_t) { return false; }
  void setNoDelay(bool) {}
  void setTimeout(uint32_t) {}
  int available() { return 0; }
  int read() { return -1; }
  size_t write(const uint8_t *, size_t) { return 0; }
  size_t readBytes(uint8_t *, size_t) { return 0; }
};
