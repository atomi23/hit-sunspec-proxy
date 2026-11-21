#pragma once
#include <Arduino.h>
#include "AppConfig.h"

// HTTPS auf Port 443 vor der Weboberfläche (die selbst auf Port 80 bleibt).
// Zertifikat: selbst signiert (automatisch erzeugt) oder ein eigenes, über die Weboberfläche hochgeladen.
namespace HttpsServer {
void begin(const AppConfig &cfg);
void applyConfig(const AppConfig &cfg);
void loop(); // startet, stoppt und wechselt Zertifikate; nur aus der Hauptschleife

bool running();
bool hasRoom(); // läuft und der interne RAM reicht für eine neue TLS-Sitzung
String infoJson();

// Prüft Zertifikat (PEM, auch mit Kette) und privaten Schlüssel; bei Erfolg gilt es nach dem nächsten loop()
bool installCustom(const char *certPem, const char *keyPem, String &err, bool english);
void useSelfSigned(); // eigenes Zertifikat löschen, neues selbst signiertes erzeugen
void forget();        // Werksreset: alles Zertifikatsmaterial löschen
}
