#pragma once
#include <Arduino.h>
#include <map>
#include <memory>

struct WiFiClient {
  struct Socket {
    bool connected = true;
    std::deque<uint8_t> rx;
    size_t bytesWritten = 0;
  };
  std::shared_ptr<Socket> socket;

  explicit operator bool() const { return connected(); }
  bool connected() const { return socket && socket->connected; }
  void stop() { if (socket) socket->connected = false; }
  bool connect(const char *, uint16_t, int32_t) { return false; }
  void setNoDelay(bool) {}
  void setTimeout(uint32_t) {}
  int available() { return connected() ? int(socket->rx.size()) : 0; }
  int read() {
    if (!available()) return -1;
    const int b = socket->rx.front();
    socket->rx.pop_front();
    return b;
  }
  size_t write(const uint8_t *, size_t n) {
    if (!connected()) return 0;
    socket->bytesWritten += n;
    return n;
  }
  size_t readBytes(uint8_t *buf, size_t n) {
    size_t got = 0;
    while (got < n && available()) buf[got++] = uint8_t(read());
    return got;
  }
};

class WiFiServer {
  uint16_t port;
  bool listening = false;

public:
  static inline std::map<uint16_t, std::deque<WiFiClient>> incoming;

  explicit WiFiServer(uint16_t port) : port(port) {}
  void begin() { listening = true; }
  void end() { listening = false; }
  WiFiClient accept() {
    auto &queue = incoming[port];
    if (!listening || queue.empty()) return {};
    WiFiClient c = queue.front();
    queue.pop_front();
    return c;
  }
};
