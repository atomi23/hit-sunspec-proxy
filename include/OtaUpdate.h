#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

namespace OtaUpdate {
  // Registriert /ota Seite + /api/ota Upload (Auth-Callback vom WebConfig)
  void registerRoutes(AsyncWebServer &server,
                      bool (*requireAuth)(AsyncWebServerRequest *));

  void loop(); // pending reboot nach erfolgreichem Flash
  bool inProgress();
  const char *lastError();
}
