#pragma once
#include "AppConfig.h"

class AsyncWebServerRequest;

namespace WebConfig {
  void begin();
  void loop();
  void sendPage(AsyncWebServerRequest *req, const char *html);
}
