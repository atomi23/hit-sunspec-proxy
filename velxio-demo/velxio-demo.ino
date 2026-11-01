/**
 * Velxio-Test: HIT-Proxy Demo (ohne echtes Board / HIT / Consolinno)
 *
 * In https://velxio.dev/editor :
 *  1. Board: ESP32-S3 DevKitC (oder ESP32 DevKit)
 *  2. Diesen Sketch einfügen → Compile → Play
 *  3. Serial Monitor öffnen (115200)
 *
 * Optional auf der Canvas: LED an GPIO 2 (viele DevKits: Onboard-LED)
 */

#include <math.h>

#ifndef LED_PIN
#define LED_PIN 2
#endif

static uint32_t nextPrint = 0;
static uint32_t nextBlink = 0;
static bool ledOn = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(LED_PIN, OUTPUT);

  Serial.println();
  Serial.println(F("=== HIT-SunSpec-Proxy DEMO (Velxio) ==="));
  Serial.println(F("Fake PV/Bat/Netz — kein Modbus, kein HIT"));
  Serial.println(F("Zum Vergleich: am echten Board Web-UI → Demo-Modus"));
  Serial.println();
}

void loop() {
  uint32_t now = millis();

  if (now >= nextBlink) {
    nextBlink = now + 400;
    ledOn = !ledOn;
    digitalWrite(LED_PIN, ledOn ? HIGH : LOW);
  }

  if (now < nextPrint) return;
  nextPrint = now + 2000;

  float t = now / 1000.0f;
  float sun = fmaxf(0.0f, sinf((t / 60.0f) * 3.1415926f));
  int pvW = (int)(800 + 4200 * sun);
  int batW = (int)(400 * sinf(t / 17.0f));
  int loadW = 1200;
  int gridW = pvW + batW - loadW;
  int soc = (int)(45 + 20 * sinf(t / 90.0f));
  if (soc < 0) soc = 0;
  if (soc > 100) soc = 100;
  double ePv = 1000.0 + t / 3600.0; // Pseudodaten

  Serial.println(F("---------- LIVE (DEMO) ----------"));
  Serial.printf("PV      %5d W\n", pvW);
  Serial.printf("Batterie%5d W   SoC %d %%\n", batW, soc);
  Serial.printf("Netz    %5d W   (%s)\n", gridW,
                gridW < -20 ? "Einspeisung" : (gridW > 20 ? "Bezug" : "~0"));
  Serial.printf("Energie %.1f kWh (fake steigend)\n", ePv);
  Serial.printf("Uptime  %lu s\n", (unsigned long)(now / 1000));
  Serial.println(F("SunSpec :1502 — nur auf echtem Board / Demo-Firmware"));
  Serial.println();
}
