#include "OtaUpdate.h"
#include "LiveData.h"
#include <Update.h>
#include "AppConfig.h"
#include "EventLog.h"
#include "HttpsServer.h"
#include "WebConfig.h"
#include "WebAssets.h"

namespace {
bool busy = false;
bool rebootPending = false;
bool uploadOk = false;
bool factoryPending = false;
uint32_t rebootAt = 0;
char errBuf[96] = "";
size_t written = 0;
// Request, der gerade flasht. Andere Uploads (auch nicht angemeldete Folge-Stücke) dürfen nicht hineinschreiben.
AsyncWebServerRequest *owner = nullptr;

} // namespace

namespace OtaUpdate {

bool inProgress() { return busy; }

void restartSoon() {
  strlcpy(gLive.status, "Neustart", sizeof(gLive.status));
  rebootAt = millis() + 1000;
  rebootPending = true;
}
const char *lastError() { return errBuf; }

void registerRoutes(AsyncWebServer &server, bool (*requireAuth)(AsyncWebServerRequest *)) {
  server.on("/ota", HTTP_GET, [requireAuth](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    WebConfig::sendPage(req, WebAssets::OTA_HTML);
  });

  server.on("/api/restart", HTTP_POST, [requireAuth](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    req->send(200, "application/json", "{\"ok\":true}");
    restartSoon();
  });

  server.on("/api/factory-reset", HTTP_POST, [requireAuth](AsyncWebServerRequest *req) {
    if (!requireAuth(req)) return;
    const AsyncWebParameter *p = req->getParam("confirm", true);
    if (!p || p->value() != "RESET") {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"confirm\"}");
      return;
    }
    factoryPending = true;
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on(
      "/api/ota", HTTP_POST,
      [requireAuth](AsyncWebServerRequest *req) {
        if (!requireAuth(req)) return;
        if (req != owner) {
          req->send(409, "application/json", ConfigStore::uiText(
              "{\"ok\":false,\"error\":\"Ein anderes Update läuft oder du bist nicht angemeldet.\"}",
              "{\"ok\":false,\"error\":\"Another update is running or you are not signed in.\"}"));
          return;
        }
        owner = nullptr;
        busy = false;
        if (!uploadOk || Update.hasError() || errBuf[0]) {
          String e = errBuf[0] ? String(errBuf) : String(Update.errorString());
          if (!e.length()) e = ConfigStore::uiText("Update fehlgeschlagen", "Update failed");
          // JSON escape grob
          e.replace("\"", "'");
          req->send(500, "application/json", String("{\"ok\":false,\"error\":\"") + e + "\"}");
          return;
        }
        strlcpy(gLive.status, "OTA OK reboot", sizeof(gLive.status));
        req->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
        rebootAt = millis() + 1000;
        rebootPending = true;
      },
      [requireAuth](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0) {
          if (!requireAuth(req)) return;
          if (busy && owner && owner != req) return; // anderer Upload läuft schon
          owner = req;
          // Verbindung weg, bevor das Bild fertig ist: Flash-Vorgang verwerfen und freigeben
          req->onDisconnect([req]() {
            if (owner != req) return;
            owner = nullptr;
            if (Update.isRunning()) Update.abort();
            busy = false;
          });
          errBuf[0] = '\0';
          uploadOk = false;
          written = 0;
          busy = true;
          strlcpy(gLive.status, "OTA flash…", sizeof(gLive.status));

          // Abgebrochener Upload (VPN/Timeout) lässt Update sonst mit „No Error“ hängen
          if (Update.isRunning()) Update.abort();

          // Multipart: Content-Length ≠ Bin-Größe → UNKNOWN
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            const char *why = Update.errorString();
            if (!why || !why[0] || strcmp(why, "No Error") == 0) why = ConfigStore::uiText(
                "OTA-Slot belegt — Gerät neu starten und erneut versuchen",
                "OTA slot busy — restart the device and try again");
            snprintf(errBuf, sizeof(errBuf), "begin: %s", why);
            busy = false;
            return;
          }
          Serial.printf("OTA begin %s freeSketch=%u\n", filename.c_str(), (unsigned)ESP.getFreeSketchSpace());
        }

        if (req != owner || errBuf[0] || !busy) return;

        if (len) {
          if (Update.write(data, len) != len) {
            snprintf(errBuf, sizeof(errBuf), "write@%u: %s", (unsigned)written, Update.errorString());
            busy = false;
            return;
          }
          written += len;
        }

        if (final) {
          if (errBuf[0]) {
            Update.abort();
            busy = false;
            return;
          }
          if (!Update.end(true)) {
            snprintf(errBuf, sizeof(errBuf), "end: %s", Update.errorString());
            busy = false;
            uploadOk = false;
            return;
          }
          uploadOk = true;
          Serial.printf("OTA OK %u bytes\n", (unsigned)written);
          EventLog::add('I', "OTA", ConfigStore::uiText("Firmware übertragen (%u Bytes), Neustart folgt",
                                                       "Firmware uploaded (%u bytes); restarting"), (unsigned)written);
        }
      });
}

void loop() {
  if (factoryPending) {
    delay(500);
    Serial.println("Werksreset");
    ConfigStore::factoryReset();
    HttpsServer::forget();
    ESP.restart();
  }
  if (!rebootPending) return;
  if (int32_t(millis() - rebootAt) < 0) return;
  Serial.println("OTA reboot now");
  ESP.restart();
}

} // namespace OtaUpdate
