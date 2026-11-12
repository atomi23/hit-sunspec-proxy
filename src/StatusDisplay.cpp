#include "StatusDisplay.h"

#if defined(BOARD_T_CONNECT_PRO)
#include "LgfxBoard.h"
#include "DebugLog.h"
#include "DispFonts.h"
static LgfxBoard panel;
static LGFX_Sprite canvas(&panel);
static lgfx::LovyanGFX *gfx = &panel; // canvas, falls Puffer allokiert
static bool buffered = false;
static bool ready = false;
static uint8_t page = 0;
static constexpr uint8_t PAGES = 3;
static uint32_t nextPage = 0;
static uint32_t nextDraw = 0;
static uint32_t lastActivity = 0;
static bool asleep = false;
static volatile bool tapRequested = false;
// fertige Bilder für die Web-Ansicht; der Zeichenpuffer selbst ist mitten im Neuzeichnen unvollständig
static uint8_t *snap = nullptr;
static size_t snapLen = 0;
static volatile uint32_t snapLockedAt = 0; // während einer Übertragung nicht überschreiben
static AppConfig local;

static lgfx::PointerWrapper wHero, wVal, wLbl, wSm;
static lgfx::VLWfont fHero, fVal, fLbl, fSm;

static void loadFonts() {
  wHero.set(DFONT_HERO, sizeof(DFONT_HERO)); fHero.loadFont(&wHero);
  wVal.set(DFONT_VAL, sizeof(DFONT_VAL));    fVal.loadFont(&wVal);
  wLbl.set(DFONT_LBL, sizeof(DFONT_LBL));    fLbl.loadFont(&wLbl);
  wSm.set(DFONT_SM, sizeof(DFONT_SM));       fSm.loadFont(&wSm);
}

static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return uint16_t(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct Palette { uint16_t bg, card, rule, ink, dim, acc, sun, grid, warn, fault; };
static Palette P;

static void applyThemeColors() {
  switch (local.uiTheme) {
    case UiTheme::Ocean:
      P = {rgb(0x12, 0x1a, 0x22), rgb(0x1e, 0x2a, 0x36), rgb(0x2e, 0x3d, 0x4d), rgb(0xe8, 0xee, 0xf4),
           rgb(0x8d, 0x9e, 0xb0), rgb(0x79, 0xc7, 0xf2), rgb(0xf1, 0xc4, 0x53), rgb(0x9f, 0xb4, 0xf5),
           rgb(0xf0, 0xa3, 0x5e), rgb(0xef, 0x6f, 0x62)};
      break;
    case UiTheme::Ember:
      P = {rgb(0x1c, 0x16, 0x15), rgb(0x2b, 0x23, 0x21), rgb(0x3e, 0x33, 0x30), rgb(0xf3, 0xec, 0xe6),
           rgb(0xa3, 0x93, 0x89), rgb(0xf0, 0xa3, 0x5e), rgb(0xf1, 0xc4, 0x53), rgb(0x8a, 0xb8, 0xe6),
           rgb(0xf1, 0xc4, 0x53), rgb(0xef, 0x6f, 0x62)};
      break;
    case UiTheme::Light:
      P = {rgb(0xf2, 0xf3, 0xef), rgb(0xff, 0xff, 0xff), rgb(0xdf, 0xe3, 0xdc), rgb(0x1a, 0x1f, 0x22),
           rgb(0x69, 0x73, 0x6c), rgb(0x1f, 0x9d, 0x6b), rgb(0xc9, 0x8a, 0x00), rgb(0x2f, 0x7f, 0xc4),
           rgb(0xc7, 0x6f, 0x12), rgb(0xd1, 0x44, 0x34)};
      break;
    default:
      P = {rgb(0x15, 0x19, 0x1b), rgb(0x22, 0x28, 0x2b), rgb(0x33, 0x3a, 0x3e), rgb(0xed, 0xef, 0xec),
           rgb(0x8f, 0x98, 0x93), rgb(0x8e, 0xd8, 0xb4), rgb(0xf1, 0xc4, 0x53), rgb(0x8a, 0xb8, 0xe6),
           rgb(0xf0, 0xa3, 0x5e), rgb(0xef, 0x6f, 0x62)};
      break;
  }
}

static bool en() { return local.uiLang == UiLang::En; }
static const char *L(const char *de, const char *enTxt) { return en() ? enTxt : de; }

// ---- Zahlen im Landesformat -------------------------------------------------

static void fmtInt(char *out, size_t n, long v) {
  char raw[16];
  snprintf(raw, sizeof(raw), "%ld", v < 0 ? -v : v);
  const char sep = en() ? ',' : '.';
  size_t len = strlen(raw), o = 0;
  if (v < 0 && o + 1 < n) out[o++] = '-';
  for (size_t i = 0; i < len && o + 1 < n; i++) {
    out[o++] = raw[i];
    size_t rest = len - i - 1;
    if (rest && rest % 3 == 0 && o + 1 < n) out[o++] = sep;
  }
  out[o] = 0;
}

static void fmtDec(char *out, size_t n, double v, int digits) {
  snprintf(out, n, "%.*f", digits, v);
  if (!en()) for (char *p = out; *p; p++) if (*p == '.') *p = ',';
}

static const char *originText(DataOrigin o) {
  switch (o) {
    case DataOrigin::Native: return L("HIT direkt", "HIT direct");
    case DataOrigin::Mqtt: return "MQTT";
    case DataOrigin::Cloud: return "Hoymiles-Cloud";
    case DataOrigin::Demo: return "Demo";
    default: return L("keine", "none");
  }
}

static void fmtAge(char *out, size_t n, uint32_t sinceMs) {
  if (!sinceMs) { snprintf(out, n, "%s", L("keine Daten", "no data")); return; }
  uint32_t s = (millis() - sinceMs) / 1000;
  if (s < 90) snprintf(out, n, en() ? "%lus ago" : "vor %lu s", (unsigned long)s);
  else snprintf(out, n, en() ? "%lu min ago" : "vor %lu min", (unsigned long)(s / 60));
}

// ---- Zeichenhelfer ----------------------------------------------------------

static void text(const lgfx::IFont &f, uint16_t fg, uint16_t bg, textdatum_t d, const char *s, int x, int y) {
  gfx->setFont(&f);
  gfx->setTextSize(1);
  gfx->setTextDatum(d);
  gfx->setTextColor(fg, bg);
  gfx->drawString(s, x, y);
}

static int width(const lgfx::IFont &f, const char *s) {
  gfx->setFont(&f);
  gfx->setTextSize(1);
  return gfx->textWidth(s);
}

static void card(int x, int y, int w, int h) { gfx->fillRoundRect(x, y, w, h, 14, P.card); }

// Statuspunkt mit Label, rechtsbündig; liefert die nächste freie x-Position
static int status(int xRight, int y, bool ok, const char *label) {
  int w = width(fSm, label);
  text(fSm, ok ? P.ink : P.dim, P.bg, middle_right, label, xRight, y);
  gfx->fillCircle(xRight - w - 8, y, 3, ok ? P.acc : P.fault);
  return xRight - w - 26;
}

static void topBar(const char *title) {
  text(fSm, P.dim, P.bg, middle_left, title, 16, 14);
  int x = 464;
  x = status(x, 14, gLive.ethOk || gLive.wifiOk, L("Netzwerk", "Network"));
  x = status(x, 14, gLive.sunspecClient, "Consolinno");
  x = status(x, 14, gLive.hitOk, "HIT");
  if (local.demoMode || gLive.dataOrigin == DataOrigin::Demo) {
    int w = width(fSm, "Demo") + 18;
    gfx->drawRoundRect(x - w + 6, 5, w, 19, 9, P.warn);
    text(fSm, P.warn, P.bg, middle_center, "Demo", x - w / 2 + 6, 14);
  }
}

static void footer() {
  for (int i = 0; i < PAGES; i++) {
    int cx = 240 + (i - 1) * 14;
    if (i == page) gfx->fillRoundRect(cx - 6, 209, 12, 5, 2, P.acc);
    else gfx->fillCircle(cx, 211, 2, P.rule);
  }
  char age[24];
  uint32_t dataAt = 0; // Alter der Messwerte aus der aktiven Quelle
  switch (gLive.dataOrigin) {
    case DataOrigin::Native:
    case DataOrigin::Demo: dataAt = gLive.lastHitMs; break;
    case DataOrigin::Mqtt: dataAt = gLive.lastMqttIngestMs; break;
    case DataOrigin::Cloud: dataAt = gLive.lastCloudMs; break;
    default: break;
  }
  fmtAge(age, sizeof(age), dataAt);
  text(fSm, P.dim, P.bg, middle_right, age, 464, 211);
  if (gLive.dataOrigin != DataOrigin::Native && gLive.dataOrigin != DataOrigin::None) {
    char src[40];
    snprintf(src, sizeof(src), "%s %s", L("Quelle", "Source"), originText(gLive.dataOrigin));
    text(fSm, P.warn, P.bg, middle_left, src, 16, 211);
  }
}

// Zeile im Fluss-Panel: Farbpunkt, Name, Zustand, Wert
static void flowRow(int y, uint16_t dot, const char *name, const char *state, long watts) {
  gfx->fillCircle(270, y - 6, 4, dot);
  text(fLbl, P.ink, P.card, middle_left, name, 282, y - 7);
  text(fSm, P.dim, P.card, middle_left, state, 282, y + 10);
  char v[20], num[16];
  fmtInt(num, sizeof(num), watts);
  snprintf(v, sizeof(v), "%s W", num);
  text(fVal, P.ink, P.card, middle_right, v, 454, y + 1);
}

// ---- Seiten -----------------------------------------------------------------

static void drawPageLive() {
  topBar("HIT-Proxy");

  char num[20], line[48];
  gfx->fillCircle(21, 47, 4, P.sun);
  text(fLbl, P.dim, P.bg, middle_left, L("PV-Leistung", "Solar power"), 32, 46);
  fmtInt(num, sizeof(num), gLive.pvW);
  text(fHero, P.ink, P.bg, baseline_left, num, 14, 118);
  text(fVal, P.dim, P.bg, baseline_left, "W", 14 + width(fHero, num) + 8, 118);
  fmtInt(num, sizeof(num), long(gLive.energyPvKwh + 0.5));
  snprintf(line, sizeof(line), "%s %s kWh", L("Ertrag gesamt", "Total yield"), num);
  text(fSm, P.dim, P.bg, middle_left, line, 16, 142);

  card(252, 30, 216, 122);
  const long bat = gLive.batW, grid = gLive.gridW;
  const long house = long(gLive.pvW) + bat + grid;
  flowRow(54, P.acc, L("Akku", "Battery"),
          bat < -20 ? L("lädt", "charging") : bat > 20 ? L("entlädt", "discharging") : L("ruht", "idle"),
          bat < 0 ? -bat : bat);
  gfx->drawFastHLine(266, 71, 188, P.rule);
  flowRow(93, P.grid, L("Netz", "Grid"),
          grid > 20 ? L("Bezug", "import") : grid < -20 ? L("Einspeisung", "export") : L("ausgeglichen", "balanced"),
          grid < 0 ? -grid : grid);
  gfx->drawFastHLine(266, 110, 188, P.rule);
  flowRow(132, P.ink, L("Haus", "Home"), L("Verbrauch", "consumption"), house < 0 ? 0 : house);

  card(12, 160, 456, 40);
  text(fLbl, P.dim, P.card, middle_left, L("Ladestand", "Charge"), 28, 180);
  snprintf(line, sizeof(line), "%u %%", unsigned(gLive.soc));
  text(fVal, P.ink, P.card, middle_left, line, 118, 181);
  const int bx = 200, bw = 252;
  const float pct = gLive.soc > 100 ? 1.0f : gLive.soc / 100.0f;
  const uint16_t col = gLive.soc < 15 ? P.fault : gLive.soc < 30 ? P.warn : P.acc;
  gfx->fillRoundRect(bx, 176, bw, 8, 4, P.rule);
  int fw = int(bw * pct);
  if (fw >= 8) gfx->fillRoundRect(bx, 176, fw, 8, 4, col);

  footer();
}

static void kv(int x, int xr, int y, uint16_t bg, const char *k, const char *v, uint16_t vc) {
  text(fLbl, P.dim, bg, middle_left, k, x, y);
  text(fLbl, vc, bg, middle_right, v, xr, y);
}

static void drawPageDetails() {
  topBar(L("Strings und Netz", "Strings and grid"));
  char v[40], a[16], b[16];

  card(12, 30, 222, 170);
  text(fSm, P.dim, P.card, middle_left, L("PV-Strings", "PV strings"), 28, 46);
  const int32_t sw[4] = {gLive.pv1W, gLive.pv2W, gLive.pv3W, gLive.pv4W};
  const float sv[4] = {gLive.pv1V, gLive.pv2V, gLive.pv3V, gLive.pv4V};
  for (int i = 0; i < 4; i++) {
    int y = 72 + i * 28;
    char k[16];
    snprintf(k, sizeof(k), "String %d", i + 1);
    text(fLbl, P.dim, P.card, middle_left, k, 28, y);
    fmtInt(a, sizeof(a), long(sv[i] + 0.5f));
    snprintf(v, sizeof(v), "%s V", a);
    text(fSm, P.dim, P.card, middle_right, v, 150, y + 1);
    fmtInt(a, sizeof(a), sw[i]);
    snprintf(v, sizeof(v), "%s W", a);
    text(fLbl, sw[i] > 0 ? P.ink : P.dim, P.card, middle_right, v, 218, y);
  }
  gfx->drawFastHLine(28, 172, 190, P.rule);
  fmtInt(a, sizeof(a), gLive.pvW);
  snprintf(v, sizeof(v), "%s W", a);
  kv(28, 218, 186, P.card, L("Summe", "Total"), v, P.sun);

  card(246, 30, 222, 170);
  text(fSm, P.dim, P.card, middle_left, L("Netz je Phase", "Grid per phase"), 262, 46);
  const float pv[3] = {gLive.v1, gLive.v2, gLive.v3};
  const float pi[3] = {gLive.i1, gLive.i2, gLive.i3};
  for (int i = 0; i < 3; i++) {
    int y = 72 + i * 28;
    char k[4];
    snprintf(k, sizeof(k), "L%d", i + 1);
    fmtDec(a, sizeof(a), pv[i], 1);
    fmtDec(b, sizeof(b), pi[i], 1);
    snprintf(v, sizeof(v), "%s V   %s A", a, b);
    kv(262, 452, y, P.card, k, v, P.ink);
  }
  fmtDec(a, sizeof(a), gLive.freqHz, 2);
  snprintf(v, sizeof(v), "%s Hz", a);
  kv(262, 452, 156, P.card, L("Frequenz", "Frequency"), v, P.ink);
  gfx->drawFastHLine(262, 172, 190, P.rule);
  const long meter = gLive.tasmotaOk ? gLive.tasmotaPowerW : gLive.gridW;
  fmtInt(a, sizeof(a), meter < 0 ? -meter : meter);
  snprintf(v, sizeof(v), "%s W %s", a, meter < 0 ? L("Einsp.", "export") : L("Bezug", "import"));
  kv(262, 452, 186, P.card, gLive.tasmotaOk ? L("Zähler", "Meter") : L("Netz", "Grid"), v, P.grid);

  footer();
}

static void cell(int x, int y, const char *k, const char *v, uint16_t vc) {
  text(fSm, P.dim, P.card, top_left, k, x, y);
  text(fLbl, vc, P.card, top_left, v, x, y + 17);
}

static void drawPageSystem() {
  topBar("System");
  card(12, 30, 456, 170);
  char v[48], age[24];

  cell(28, 42, L("Datenquelle", "Data source"), originText(gLive.dataOrigin),
       gLive.dataOrigin == DataOrigin::Native ? P.acc : P.warn);
  fmtAge(age, sizeof(age), gLive.lastHitMs);
  if (gLive.hitFails)
    snprintf(v, sizeof(v), L("%s, %lu/%lu Fehler", "%s, %lu/%lu errors"), age, (unsigned long)gLive.hitFails,
             (unsigned long)gLive.hitPolls);
  else
    snprintf(v, sizeof(v), "%s", age);
  cell(248, 42, L("HIT-Antwort", "HIT response"), v, gLive.hitOk ? P.ink : P.fault);

  cell(28, 82, "Consolinno", gLive.sunspecClient ? L("fragt ab", "polling") : L("wartet", "waiting"),
       gLive.sunspecClient ? P.ink : P.dim);
  if (gLive.wMaxLimEna) {
    fmtDec(v, sizeof(v), gLive.wMaxLimPct / 100.0, 0);
    strncat(v, " %", sizeof(v) - strlen(v) - 1);
  } else {
    snprintf(v, sizeof(v), "%s", L("keine", "none"));
  }
  cell(248, 82, L("Leistungsbegrenzung", "Power limit"), v, gLive.wMaxLimEna ? P.warn : P.ink);

  if (DebugLog::lastWriteMs()) {
    fmtAge(age, sizeof(age), DebugLog::lastWriteMs());
    snprintf(v, sizeof(v), "%s", age);
  } else {
    snprintf(v, sizeof(v), "%s", L("noch keiner", "none yet"));
  }
  cell(28, 122, L("Letzter Schreibbefehl", "Last write"), v, P.ink);
  snprintf(v, sizeof(v), "MQTT %s  KNX %s", gLive.mqttOk ? "ok" : "–", gLive.knxOk ? "ok" : "–");
  cell(248, 122, L("Dienste", "Services"), v, P.ink);

  cell(28, 162, L("Adresse", "Address"), gLive.ipShown, P.ink);
  cell(248, 162, L("Zugangscode", "Access code"), local.displayCode[0] ? local.displayCode : local.wifiPass, P.acc);

  footer();
}

static void drawSetupScreen() {
  gfx->fillScreen(P.bg);
  const char *code = local.displayCode[0] ? local.displayCode : local.wifiPass;

  // Mit Kabel ist die Einrichtung auch direkt aus dem Heimnetz erreichbar
  const bool cable = gLive.ethOk && strcmp(gLive.ipEth, "-") != 0;
  text(fVal, P.ink, P.bg, top_left, L("Einrichtung", "Setup"), 16, 12);
  text(fSm, P.dim, P.bg, top_left,
       cable ? L("Per Kabel oder Geräte-WLAN", "Via cable or device Wi-Fi")
             : L("Mit dem WLAN des Geräts verbinden", "Join the device Wi-Fi"),
       16, 42);

  cell(16, 68, L("WLAN", "Wi-Fi"), local.wifiSsid, P.ink);
  text(fSm, P.dim, P.bg, top_left, L("Passwort", "Password"), 16, 110);
  text(fVal, P.acc, P.bg, top_left, code, 16, 126);
  char url[40];
  snprintf(url, sizeof(url), "http://%s", cable ? gLive.ipEth : gLive.ipShown);
  text(fSm, P.dim, P.bg, top_left, L("Im Browser öffnen", "Open in a browser"), 16, 164);
  text(fLbl, P.ink, P.bg, top_left, url, 16, 181);
  char login[64];
  if (cable)
    snprintf(login, sizeof(login), "%s http://%s - Login %s", L("WLAN:", "Wi-Fi:"), gLive.ipWifi, local.webUser);
  else
    snprintf(login, sizeof(login), "Login %s", local.webUser);
  text(fSm, P.dim, P.bg, top_left, login, 16, 202);

  char wifiQr[128];
  snprintf(wifiQr, sizeof(wifiQr), "WIFI:T:WPA;S:%s;P:%s;;", local.wifiSsid, code);
  gfx->fillRoundRect(300, 14, 166, 166, 16, 0xFFFF);
  gfx->qrcode(wifiQr, 311, 25, 144, 4);
  text(fSm, P.dim, P.bg, top_center, L("Mit der Kamera scannen", "Scan with your camera"), 383, 190);
}

static void redraw() {
  if (!local.setupComplete) {
    drawSetupScreen();
  } else {
    gfx->fillScreen(P.bg);
    if (page == 0) drawPageLive();
    else if (page == 1) drawPageDetails();
    else drawPageSystem();
  }
  if (buffered) canvas.pushSprite(0, 0);
  const bool locked = snapLockedAt && millis() - snapLockedAt < 5000;
  if (snap && !locked) memcpy(snap, canvas.getBuffer(), snapLen);
}

static void allocCanvas() {
  const int w = panel.width(), h = panel.height();
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  if (!canvas.createSprite(w, h)) {
    // ohne PSRAM: 8-Bit passt in den internen RAM (~107 KB)
    canvas.setPsram(false);
    canvas.setColorDepth(8);
    canvas.createSprite(w, h);
  }
  buffered = canvas.getBuffer() != nullptr;
  gfx = buffered ? static_cast<lgfx::LovyanGFX *>(&canvas) : &panel;
  if (buffered && canvas.getColorDepth() == 16) {
    snapLen = size_t(w) * h * 2;
    snap = static_cast<uint8_t *>(heap_caps_malloc(snapLen, MALLOC_CAP_SPIRAM));
    if (snap) memset(snap, 0, snapLen);
  }
  Serial.printf("Display: %s\n", buffered ? (canvas.getColorDepth() == 16 ? "Puffer 16 Bit" : "Puffer 8 Bit")
                                          : "ohne Puffer");
}

namespace StatusDisplay {

void begin(const AppConfig &cfg) {
  local = cfg;
  applyThemeColors();
  gLive.setupScreen = !cfg.setupComplete;
  if (!cfg.displayEnabled) return;
  ready = panel.init();
  if (!ready) return;
  panel.setRotation(3);
  panel.setBrightness(cfg.displayBrightness);
  panel.fillScreen(P.bg);
  loadFonts();
  allocCanvas();
  redraw();
  nextPage = millis() + cfg.displayPageMs;
  nextDraw = millis() + 1000;
  lastActivity = millis();
}

void setBrightness(uint8_t level) {
  if (ready && !asleep) panel.setBrightness(level);
}

static void wake() {
  lastActivity = millis();
  if (!asleep) return;
  asleep = false;
  redraw();
  panel.setBrightness(local.displayBrightness);
}

void applyConfig(const AppConfig &cfg) {
  local = cfg;
  applyThemeColors();
  gLive.setupScreen = !cfg.setupComplete;
  if (!ready) return;
  wake();
  setBrightness(cfg.displayBrightness);
  nextPage = millis() + cfg.displayPageMs;
  redraw();
}

const uint8_t *frame(size_t &len, int &w, int &h) {
  if (!ready || !snap) return nullptr;
  w = canvas.width();
  h = canvas.height();
  len = snapLen;
  snapLockedAt = millis() | 1;
  return snap;
}

void frameDone() { snapLockedAt = 0; }

void requestTap() { tapRequested = true; }

void loop() {
  if (!ready) return;

  if (!local.setupComplete) {
    wake();
    if (millis() >= nextDraw) {
      nextDraw = millis() + 1500;
      redraw();
    }
    return;
  }

  // Seitenwechsel nur beim Antippen, nicht solange der Finger liegt
  static bool touchDown = false;
  int32_t x, y;
  bool touched = panel.getTouch(&x, &y);
  const bool webTap = tapRequested;
  tapRequested = false;
  if ((touched && !touchDown) || webTap) {
    if (asleep) {
      wake(); // erstes Antippen weckt nur
    } else {
      lastActivity = millis();
      page = (page + 1) % PAGES;
      nextPage = millis() + local.displayPageMs;
      redraw();
    }
  }
  touchDown = touched;

  // Störung hält das Display an, damit man sie im Schaltschrank sieht
  if (!local.demoMode && !gLive.hitOk) wake();
  if (local.displayTimeoutS && !asleep && millis() - lastActivity > local.displayTimeoutS * 1000UL) {
    asleep = true;
    panel.setBrightness(0);
  }
  // auch mit ausgeschaltetem Licht weiterzeichnen: die Web-Live-Ansicht liest den Puffer

  if (local.displayAutoPage && millis() >= nextPage) {
    nextPage = millis() + local.displayPageMs;
    page = (page + 1) % PAGES;
    redraw();
  } else if (!local.displayAutoPage && page != 0 && millis() - lastActivity > 30000) {
    page = 0; // ohne Automatik nach dem Blättern zurück zur Live-Seite
    redraw();
  } else if (millis() >= nextDraw) {
    nextDraw = millis() + 1000;
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
const uint8_t *frame(size_t &, int &, int &) { return nullptr; }
void frameDone() {}
void requestTap() {}
}

#endif
