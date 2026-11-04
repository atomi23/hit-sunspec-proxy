#include "StatusDisplay.h"

#if defined(BOARD_T_CONNECT_PRO)
#include "LgfxBoard.h"
#include "DebugLog.h"
static LgfxBoard lcd;
static bool ready = false;
static uint8_t page = 0;
static uint32_t nextPage = 0;
static uint32_t nextDraw = 0;
static AppConfig local;

static const uint16_t COL_BG = 0x0841;
static const uint16_t COL_CARD = 0x10A2;
static const uint16_t COL_ACCENT = 0x07E0;
static const uint16_t COL_WARN = 0xFD20;
static const uint16_t COL_BAD = 0xF800;
static const uint16_t COL_MUTED = 0x8410;
static const uint16_t COL_TEXT = 0xFFFF;
static const uint16_t COL_CYAN = 0x07FF;
static const uint16_t COL_PURPLE = 0xB01F;

static void roundCard(int x, int y, int w, int h) {
  lcd.fillRoundRect(x, y, w, h, 10, COL_CARD);
}

static void badge(int x, int y, bool ok, const char *label) {
  uint16_t c = ok ? COL_ACCENT : COL_BAD;
  lcd.fillCircle(x, y, 4, c);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_TEXT, COL_BG);
  lcd.setTextSize(1);
  lcd.drawString(label, x + 8, y);
}

static void metric(int x, int y, int w, const char *title, const String &value, const char *unit) {
  roundCard(x, y, w, 58);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.setTextSize(1);
  lcd.drawString(title, x + 10, y + 8);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextSize(2);
  lcd.drawString(value, x + 10, y + 26);
  lcd.setTextSize(1);
  lcd.setTextColor(COL_CYAN, COL_CARD);
  lcd.drawString(unit, x + w - 36, y + 34);
}

static void bar(int x, int y, int w, int h, float pct, uint16_t col) {
  if (pct < 0) pct = 0;
  if (pct > 1) pct = 1;
  lcd.fillRoundRect(x, y, w, h, 4, 0x2104);
  int fw = int(w * pct);
  if (fw > 0) lcd.fillRoundRect(x, y, fw, h, 4, col);
}

static void drawHeader() {
  lcd.fillRect(0, 0, 480, 40, COL_BG);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_MUTED, COL_BG);
  lcd.setTextSize(1);
  lcd.drawString(local.deviceName, 12, 10);
  lcd.setTextColor(COL_CYAN, COL_BG);
  lcd.setTextSize(2);
  lcd.drawString(gLive.ipShown, 12, 28);
  badge(220, 12, gLive.hitOk, "HIT");
  badge(270, 12, gLive.sunspecClient, "HEMS");
  badge(330, 12, gLive.ethOk || gLive.wifiOk, "NET");
  // Datenquelle groß sichtbar
  lcd.setTextDatum(middle_right);
  uint16_t sc = COL_MUTED;
  if (gLive.dataOrigin == DataOrigin::Native) sc = COL_ACCENT;
  else if (gLive.dataOrigin == DataOrigin::Mqtt) sc = COL_CYAN;
  else if (gLive.dataOrigin == DataOrigin::Cloud) sc = COL_WARN;
  else if (gLive.dataOrigin == DataOrigin::Demo) sc = COL_PURPLE;
  lcd.setTextColor(sc, COL_BG);
  lcd.setTextSize(2);
  lcd.drawString(dataOriginLabel(gLive.dataOrigin), 468, 22);
}

static void drawPageLive() {
  drawHeader();
  metric(12, 52, 148, "PV", String(gLive.pvW), "W");
  metric(168, 52, 148, "Batterie", String(gLive.batW), "W");
  metric(324, 52, 144, "SoC", String(gLive.soc), "%");

  roundCard(12, 120, 456, 90);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.setTextSize(1);
  lcd.setTextDatum(top_left);
  lcd.drawString("Auslastung WR (20 kW)", 22, 128);
  float pct = fabsf(float(gLive.pvW)) / 20000.0f;
  bar(22, 148, 436, 12, pct, COL_ACCENT);

  // Puls-Punkte auf dem Balken
  if (pct > 0.01f) {
    int span = max(1, int(436 * pct));
    int off = int((millis() / 30) % span);
    lcd.fillCircle(22 + off, 154, 3, COL_CYAN);
  }

  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextSize(2);
  char line[64];
  snprintf(line, sizeof(line), "%.1f kWh gesamt", gLive.energyPvKwh);
  lcd.drawString(line, 22, 172);
  lcd.setTextSize(1);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.drawString("Seite 1/4  Live", 360, 188);
}

static void drawFlowDots(int x0, int x1, int y, bool leftToRight, uint16_t col) {
  int span = x1 - x0;
  if (span < 8) return;
  uint32_t t = millis() / 35;
  for (int i = 0; i < 5; i++) {
    int off = int((t + i * 28) % span);
    if (!leftToRight) off = span - off;
    lcd.fillCircle(x0 + off, y, 4, col);
  }
  lcd.drawFastHLine(x0, y, span, COL_MUTED);
}

static void drawPageFlow() {
  drawHeader();

  // Mini-Schema PV / Bat / Haus / Netz
  roundCard(12, 48, 456, 50);
  lcd.setTextDatum(middle_center);
  lcd.setTextSize(1);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.drawString("PV", 60, 62);
  lcd.drawString("BAT", 160, 62);
  lcd.drawString("HAUS", 280, 62);
  lcd.drawString("NETZ", 400, 62);
  // PV -> Haus
  if (gLive.pvW > 50) drawFlowDots(80, 250, 78, true, COL_ACCENT);
  // Bat <-> Haus
  if (gLive.batW > 50) drawFlowDots(180, 260, 78, true, COL_CYAN);       // Entladen
  else if (gLive.batW < -50) drawFlowDots(180, 260, 78, false, COL_PURPLE); // Laden

  int32_t flowW = gLive.gridW;
  if (gLive.tasmotaOk) flowW = gLive.tasmotaPowerW; // Zähler bevorzugen für Animation

  roundCard(12, 108, 456, 100);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextSize(1);
  lcd.drawString("HAUS", 70, 128);
  lcd.drawString("NETZ", 410, 128);
  const bool exportDir = flowW < -20;
  const bool importDir = flowW > 20;
  if (exportDir) drawFlowDots(110, 360, 128, true, COL_ACCENT);
  else if (importDir) drawFlowDots(110, 360, 128, false, COL_WARN);
  else lcd.drawFastHLine(110, 128, 250, COL_MUTED);

  lcd.setTextDatum(top_center);
  lcd.setTextColor(exportDir ? COL_ACCENT : (importDir ? COL_WARN : COL_MUTED), COL_CARD);
  lcd.setTextSize(2);
  char buf[48];
  if (exportDir) snprintf(buf, sizeof(buf), "Einspeisung %ld W", long(-flowW));
  else if (importDir) snprintf(buf, sizeof(buf), "Bezug %ld W", long(flowW));
  else snprintf(buf, sizeof(buf), "Netz ~0 W");
  lcd.drawString(buf, 240, 150);
  lcd.setTextSize(1);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  char src[48];
  snprintf(src, sizeof(src), "Daten: %s%s", dataOriginLabel(gLive.dataOrigin),
           gLive.tasmotaOk ? " · Zaehler Tasmota" : "");
  lcd.drawString(src, 240, 180);
  lcd.drawString("Seite 2/4  Fluss", 400, 190);
}

static void drawPageMeter() {
  drawHeader();
  metric(12, 52, 148, "Bezug HIT", String(gLive.energyImpKwh, 1), "kWh");
  metric(168, 52, 148, "Einsp. HIT", String(gLive.energyExpKwh, 1), "kWh");
  metric(324, 52, 144, "HIT Netz", String(gLive.gridW), "W");

  roundCard(12, 120, 456, 90);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.setTextSize(1);
  char title[48];
  snprintf(title, sizeof(title), "Tasmota %s", gLive.tasmotaSensorKey[0] ? gLive.tasmotaSensorKey : "—");
  lcd.drawString(title, 22, 128);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextSize(2);
  char buf[80];
  if (gLive.tasmotaOk) {
    snprintf(buf, sizeof(buf), "%ld W", long(gLive.tasmotaPowerW));
    lcd.drawString(buf, 22, 148);
    lcd.setTextSize(1);
    lcd.setTextColor(COL_CYAN, COL_CARD);
    snprintf(buf, sizeof(buf), "E_in %.2f  E_out %.2f kWh", gLive.tasmotaTotalKwh, gLive.tasmotaExportKwh);
    lcd.drawString(buf, 22, 178);
    if (local.tasmotaMeterSpoof) {
      lcd.setTextColor(COL_ACCENT, COL_CARD);
      lcd.drawString("Meter-Spoof AN (Model 201)", 260, 178);
    }
  } else {
    lcd.drawString("aus / offline", 22, 148);
    lcd.setTextSize(1);
    lcd.setTextColor(COL_MUTED, COL_CARD);
    lcd.drawString("Web-UI: One-Click Tasmota", 22, 178);
  }
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.drawString("Seite 3/4  Zaehler", 360, 198);
}

static void drawPageStatus() {
  drawHeader();
  roundCard(12, 48, 456, 162);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.setTextSize(1);
  lcd.drawString("System", 22, 58);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextSize(2);
  lcd.drawString(gLive.status, 22, 78);

  char buf[96];
  uint32_t age = gLive.lastHitMs ? (millis() - gLive.lastHitMs) / 1000 : 0;
  lcd.setTextSize(1);
  lcd.setTextColor(COL_CYAN, COL_CARD);
  snprintf(buf, sizeof(buf), "Quelle %s · HIT vor %lus · Fails %lu",
           dataOriginLabel(gLive.dataOrigin), (unsigned long)age, (unsigned long)gLive.hitFails);
  lcd.drawString(buf, 22, 112);

  snprintf(buf, sizeof(buf), "MQTT-in %s · Cloud %s · KNX %s",
           gLive.mqttIngestOk ? "ok" : "—", gLive.cloudOk ? "ok" : "—",
           gLive.knxOk ? "ok" : "—");
  lcd.drawString(buf, 22, 130);
  snprintf(buf, sizeof(buf), "LPP %s %.0f%%  Write %s",
           gLive.wMaxLimEna ? "AN" : "aus",
           gLive.wMaxLimPct / 100.0f,
           local.hitWriteEnabled ? "HIT an" : "HIT aus");
  lcd.setTextColor(gLive.wMaxLimEna ? COL_WARN : COL_MUTED, COL_CARD);
  lcd.drawString(buf, 22, 148);

  if (DebugLog::lastWriteMs()) {
    uint32_t age = (millis() - DebugLog::lastWriteMs()) / 1000;
    snprintf(buf, sizeof(buf), "Letzter Write vor %lus: %s",
             (unsigned long)age, DebugLog::lastNote());
  } else {
    snprintf(buf, sizeof(buf), "Kein Consolinno-Write bisher — /debug");
  }
  lcd.setTextColor(COL_CYAN, COL_CARD);
  lcd.drawString(buf, 22, 166);

  lcd.setTextColor(COL_ACCENT, COL_CARD);
  snprintf(buf, sizeof(buf), "Code %s",
           local.displayCode[0] ? local.displayCode : local.wifiPass);
  lcd.drawString(buf, 22, 186);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.drawString("4/4", 440, 186);
}

static void drawSetupScreen() {
  lcd.fillScreen(COL_BG);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(COL_CYAN, COL_BG);
  lcd.setTextSize(2);
  lcd.drawString("Setup / First Boot", 12, 8);

  lcd.setTextColor(COL_MUTED, COL_BG);
  lcd.setTextSize(1);
  lcd.drawString("WLAN verbinden (QR oder manuell)", 12, 34);

  roundCard(12, 50, 270, 160);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.drawString("IP (Web-UI)", 22, 58);
  lcd.setTextColor(COL_CYAN, COL_CARD);
  lcd.setTextSize(2);
  lcd.drawString(gLive.ipShown, 22, 74);

  lcd.setTextSize(1);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.drawString("WLAN SSID", 22, 104);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextSize(2);
  lcd.drawString(local.wifiSsid, 22, 118);

  lcd.setTextSize(1);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  lcd.drawString("User / Display-Code", 22, 148);
  lcd.setTextColor(COL_ACCENT, COL_CARD);
  lcd.setTextSize(2);
  char cred[80];
  const char *code = local.displayCode[0] ? local.displayCode : local.wifiPass;
  snprintf(cred, sizeof(cred), "%s / %s", local.webUser, code);
  lcd.drawString(cred, 22, 162);

  lcd.setTextSize(1);
  lcd.setTextColor(COL_MUTED, COL_CARD);
  char url[48];
  snprintf(url, sizeof(url), "http://%s/", gLive.ipShown);
  lcd.drawString(url, 22, 190);

  // WiFi QR (Handy scannt → verbindet mit AP)
  char wifiQr[128];
  const char *wpass = local.displayCode[0] ? local.displayCode : local.wifiPass;
  snprintf(wifiQr, sizeof(wifiQr), "WIFI:T:WPA;S:%s;P:%s;;", local.wifiSsid, wpass);
  lcd.fillRoundRect(292, 42, 176, 176, 8, 0xFFFF);
  lcd.setTextColor(COL_BG, 0xFFFF);
  lcd.setTextDatum(top_center);
  lcd.drawString("WLAN QR", 380, 48);
  lcd.qrcode(wifiQr, 306, 66, 148, 4);
}

static void redraw() {
  if (!local.setupComplete) {
    drawSetupScreen();
    return;
  }
  lcd.fillScreen(COL_BG);
  if (page == 0) drawPageLive();
  else if (page == 1) drawPageFlow();
  else if (page == 2) drawPageMeter();
  else drawPageStatus();
}

namespace StatusDisplay {

void begin(const AppConfig &cfg) {
  local = cfg;
  gLive.setupScreen = !cfg.setupComplete;
  if (!cfg.displayEnabled) return;
  ready = lcd.init();
  if (!ready) return;
  lcd.setRotation(1);
  lcd.setBrightness(cfg.displayBrightness);
  lcd.fillScreen(COL_BG);
  redraw();
  nextPage = millis() + cfg.displayPageMs;
  nextDraw = millis() + 80;
}

void setBrightness(uint8_t level) {
  if (ready) lcd.setBrightness(level);
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
  gLive.setupScreen = !cfg.setupComplete;
  setBrightness(cfg.displayBrightness);
  if (ready) redraw();
}

void loop() {
  if (!ready) return;

  // Setup-Screen: IP ggf. nachziehen, kein Page-Flip
  if (!local.setupComplete) {
    if (millis() >= nextDraw) {
      nextDraw = millis() + 1500;
      redraw();
    }
    return;
  }

  int32_t x, y;
  if (lcd.getTouch(&x, &y)) {
    page = (page + 1) % 4;
    nextPage = millis() + local.displayPageMs;
    redraw();
    delay(250);
  }

  if (millis() >= nextPage) {
    nextPage = millis() + local.displayPageMs;
    page = (page + 1) % 4;
    redraw();
  } else if (millis() >= nextDraw) {
    nextDraw = millis() + ((page == 0 || page == 1) ? 70 : 1000);
    redraw();
  }
}

} // namespace StatusDisplay

#else

namespace StatusDisplay {
void begin(const AppConfig &) {}
void loop() {}
void setBrightness(uint8_t) {}
void applyConfig(const AppConfig &) {}
}

#endif
