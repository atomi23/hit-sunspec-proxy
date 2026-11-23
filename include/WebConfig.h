#pragma once
#include "AppConfig.h"

class AsyncWebServerRequest;

namespace WebConfig {
  void begin();
  void loop();
  void sendGzip(AsyncWebServerRequest *req, const char *type, const uint8_t *data, size_t size);

  template <size_t N>
  void sendPage(AsyncWebServerRequest *req, const uint8_t (&html)[N]) {
    sendGzip(req, "text/html; charset=utf-8", html, N);
  }
}
