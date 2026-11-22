#include "History.h"
#include "AppConfig.h"
#include "LiveData.h"
#include <LittleFS.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <freertos/semphr.h>
#include <memory>
#include <new>

namespace {
constexpr int SLOTS = 288;           // 24 h à 5 min
constexpr uint32_t SLOT_S = 300;
constexpr int MAX_DAYS = 400;
constexpr int MAX_MONTHS = 240;
constexpr uint32_t MAGIC = 0x48495354; // "HIST"
constexpr uint16_t VERSION = 1;
constexpr int16_t NO_DATA = INT16_MIN;

struct Sample {
  int16_t pv, bat, grid, house; // W
};

struct Bar {
  uint32_t key; // yyyymmdd bzw. yyyymm
  float pv, imp, exp, chg, dis, house; // kWh
};

struct Store {
  uint32_t magic;
  uint16_t version;
  uint16_t nDays, nMonths, pad;
  uint32_t dayKey;                 // heutiger Tag, yyyymmdd
  double startPv, startImp, startExp; // Zählerstände zu Tagesbeginn
  float chg, dis, house;           // heute integriert
  uint32_t slotStamp[SLOTS];       // epoch/300 je Eintrag
  Sample slots[SLOTS];
  Bar days[MAX_DAYS];
  Bar months[MAX_MONTHS];
};

Store *st = nullptr;
bool fsOk = false;
uint32_t lastIntegrateMs = 0;
uint32_t curSlot = 0;
int32_t sumPv = 0, sumBat = 0, sumGrid = 0, sumHouse = 0;
uint32_t sumN = 0;
uint32_t nextSaveMs = 0;
bool dirty = false;
SemaphoreHandle_t lock = nullptr;
struct Guard {
  Guard() { if (lock) xSemaphoreTake(lock, portMAX_DELAY); }
  ~Guard() { if (lock) xSemaphoreGive(lock); }
};

bool haveData() {
  return gLive.dataOrigin == DataOrigin::Native || gLive.dataOrigin == DataOrigin::Mqtt ||
         gLive.dataOrigin == DataOrigin::Cloud;
}

bool countersValid() { return gLive.energyPvKwh > 1.0; }

uint32_t dateKey(const tm &t) { return uint32_t((t.tm_year + 1900) * 10000 + (t.tm_mon + 1) * 100 + t.tm_mday); }

int16_t clamp16(int32_t v) { return int16_t(constrain(v, -32000, 32000)); }

void save() {
  if (!fsOk || !st) return;
  File f = LittleFS.open("/hist.tmp", "w");
  if (!f) return;
  const size_t n = f.write(reinterpret_cast<const uint8_t *>(st), sizeof(Store));
  f.close();
  if (n == sizeof(Store)) {
    // rename ersetzt die alte Datei in einem Schritt; erst löschen ließe bei Stromausfall keine Datei übrig
    if (!LittleFS.rename("/hist.tmp", "/hist.bin")) {
      LittleFS.remove("/hist.bin");
      LittleFS.rename("/hist.tmp", "/hist.bin");
    }
    dirty = false;
  }
}

bool loadFrom(const char *path) {
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  const bool sizeOk = f.size() == sizeof(Store) &&
                      f.read(reinterpret_cast<uint8_t *>(st), sizeof(Store)) == sizeof(Store);
  f.close();
  if (sizeOk && st->magic == MAGIC && st->version == VERSION && st->nDays <= MAX_DAYS &&
      st->nMonths <= MAX_MONTHS)
    return true;
  memset(st, 0, sizeof(Store));
  return false;
}

void load() {
  // hist.tmp bleibt übrig, wenn der Strom zwischen Schreiben und Umbenennen weg war
  if (!loadFrom("/hist.bin")) loadFrom("/hist.tmp");
}

void addMonth(const Bar &d) {
  const uint32_t mk = d.key / 100;
  Bar *m = (st->nMonths && st->months[st->nMonths - 1].key == mk) ? &st->months[st->nMonths - 1] : nullptr;
  if (!m) {
    if (st->nMonths == MAX_MONTHS) {
      memmove(st->months, st->months + 1, sizeof(Bar) * (MAX_MONTHS - 1));
      st->nMonths--;
    }
    m = &st->months[st->nMonths++];
    *m = Bar{mk, 0, 0, 0, 0, 0, 0};
  }
  m->pv += d.pv; m->imp += d.imp; m->exp += d.exp;
  m->chg += d.chg; m->dis += d.dis; m->house += d.house;
}

Bar today() {
  Bar b{st->dayKey, 0, 0, 0, st->chg, st->dis, st->house};
  if (countersValid() && st->startPv > 0) {
    b.pv = float(max(0.0, gLive.energyPvKwh - st->startPv));
    b.imp = float(max(0.0, gLive.energyImpKwh - st->startImp));
    b.exp = float(max(0.0, gLive.energyExpKwh - st->startExp));
  }
  return b;
}

void startDay(uint32_t key) {
  st->dayKey = key;
  st->startPv = gLive.energyPvKwh;
  st->startImp = gLive.energyImpKwh;
  st->startExp = gLive.energyExpKwh;
  st->chg = st->dis = st->house = 0;
  dirty = true;
}

void rollDay(uint32_t key) {
  if (st->dayKey) {
    const Bar d = today();
    if (st->nDays == MAX_DAYS) {
      memmove(st->days, st->days + 1, sizeof(Bar) * (MAX_DAYS - 1));
      st->nDays--;
    }
    st->days[st->nDays++] = d;
    addMonth(d);
  }
  startDay(key);
  save();
}

void flushSlot() {
  if (!sumN || !curSlot) return;
  const int i = curSlot % SLOTS;
  st->slotStamp[i] = curSlot;
  st->slots[i] = Sample{clamp16(sumPv / int32_t(sumN)), clamp16(sumBat / int32_t(sumN)),
                        clamp16(sumGrid / int32_t(sumN)), clamp16(sumHouse / int32_t(sumN))};
  sumPv = sumBat = sumGrid = sumHouse = 0;
  sumN = 0;
  dirty = true;
}

void keyToStr(uint32_t key, bool month, char *out, size_t n) {
  if (month) snprintf(out, n, "%04lu-%02lu", (unsigned long)(key / 100), (unsigned long)(key % 100));
  else snprintf(out, n, "%04lu-%02lu-%02lu", (unsigned long)(key / 10000), (unsigned long)(key / 100 % 100),
                (unsigned long)(key % 100));
}

void putBar(JsonArray arr, const char *label, const Bar &b) {
  JsonObject o = arr.add<JsonObject>();
  o["k"] = label;
  o["pv"] = serialized(String(b.pv, 2));
  o["imp"] = serialized(String(b.imp, 2));
  o["exp"] = serialized(String(b.exp, 2));
  o["chg"] = serialized(String(b.chg, 2));
  o["dis"] = serialized(String(b.dis, 2));
  o["house"] = serialized(String(b.house, 2));
}
} // namespace

namespace History {

bool timeValid() { return time(nullptr) > 1700000000; }

void begin() {
  // Mitteleuropa mit Sommerzeit; zweiter Server ist Fallback
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.cloudflare.com");
  if (!lock) lock = xSemaphoreCreateMutex();
  st = static_cast<Store *>(heap_caps_calloc(1, sizeof(Store), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!st) st = static_cast<Store *>(calloc(1, sizeof(Store)));
  if (!st) return;
  fsOk = LittleFS.begin(true);
  if (fsOk) load();
  if (!st->magic) {
    st->magic = MAGIC;
    st->version = VERSION;
  }
  Serial.printf("Verlauf: %s, %u Tage, %u Monate\n", fsOk ? "LittleFS ok" : "ohne Speicher", st->nDays, st->nMonths);
}

void loop() {
  if (!st) return;
  Guard guard;
  const uint32_t ms = millis();
  if (ms - lastIntegrateMs < 10000) return;
  const uint32_t dtMs = lastIntegrateMs ? min<uint32_t>(ms - lastIntegrateMs, 120000) : 0;
  lastIntegrateMs = ms;
  if (!haveData() || !timeValid()) return;

  const time_t now = time(nullptr);
  tm lt;
  localtime_r(&now, &lt);
  const uint32_t key = dateKey(lt);
  if (!st->dayKey || !countersValid()) {
    if (!st->dayKey && countersValid()) startDay(key);
  } else if (key != st->dayKey) {
    rollDay(key);
  } else if (gLive.energyPvKwh + 0.05 < st->startPv) {
    startDay(key); // Zähler zurückgesetzt oder anderer HIT
  }
  if (!st->startPv && countersValid()) startDay(key);

  const int32_t pv = gLive.pvW, bat = gLive.batW, grid = gLive.gridW;
  const int32_t house = max<int32_t>(0, pv + bat + grid);
  const float h = dtMs / 3600000.0f;
  st->chg += max<int32_t>(0, -bat) * h / 1000.0f;
  st->dis += max<int32_t>(0, bat) * h / 1000.0f;
  st->house += house * h / 1000.0f;

  const uint32_t slot = uint32_t(now) / SLOT_S;
  if (slot != curSlot) {
    flushSlot();
    curSlot = slot;
  }
  sumPv += pv; sumBat += bat; sumGrid += grid; sumHouse += house;
  sumN++;
  dirty = true;

  if (dirty && int32_t(ms - nextSaveMs) >= 0) {
    nextSaveMs = ms + 10UL * 60UL * 1000UL;
    save();
  }
}

void day(JsonDocument &out) {
  out["step"] = SLOT_S;
  if (!st || !timeValid()) {
    out["valid"] = false;
    return;
  }
  const uint32_t nowSlot = uint32_t(time(nullptr)) / SLOT_S;
  out["valid"] = true;
  out["end"] = uint64_t(nowSlot) * SLOT_S;
  JsonArray pv = out["pv"].to<JsonArray>(), bat = out["bat"].to<JsonArray>();
  JsonArray grid = out["grid"].to<JsonArray>(), house = out["house"].to<JsonArray>();
  // läuft im Web-Task, flushSlot() in der Hauptschleife setzt sumN zwischendurch auf 0: einmal kopieren
  const int32_t n = int32_t(sumN);
  const int32_t sPv = sumPv, sBat = sumBat, sGrid = sumGrid, sHouse = sumHouse;
  for (int k = SLOTS - 1; k >= 0; k--) {
    const uint32_t s = nowSlot - uint32_t(k);
    const int i = s % SLOTS;
    if (s == nowSlot && n) { // laufender Slot
      pv.add(sPv / n); bat.add(sBat / n);
      grid.add(sGrid / n); house.add(sHouse / n);
    } else if (st->slotStamp[i] == s) {
      pv.add(st->slots[i].pv); bat.add(st->slots[i].bat);
      grid.add(st->slots[i].grid); house.add(st->slots[i].house);
    } else {
      pv.add(nullptr); bat.add(nullptr); grid.add(nullptr); house.add(nullptr);
    }
  }
}

void bars(const char *unit, int count, JsonDocument &out) {
  out["unit"] = unit;
  JsonArray arr = out["bars"].to<JsonArray>();
  JsonObject tot = out["total"].to<JsonObject>();
  tot["pv"] = serialized(String(gLive.energyPvKwh, 1));
  tot["imp"] = serialized(String(gLive.energyImpKwh, 1));
  tot["exp"] = serialized(String(gLive.energyExpKwh, 1));
  if (!st) return;
  char label[16];
  const bool haveToday = st->dayKey != 0;
  const Bar td = haveToday ? today() : Bar{};

  if (!strcmp(unit, "day")) {
    const int n = st->nDays;
    for (int i = max(0, n - (count - 1)); i < n; i++) {
      keyToStr(st->days[i].key, false, label, sizeof(label));
      putBar(arr, label, st->days[i]);
    }
    if (haveToday) {
      keyToStr(td.key, false, label, sizeof(label));
      putBar(arr, label, td);
    }
  } else if (!strcmp(unit, "month") || !strcmp(unit, "year")) {
    // Monate plus laufender Monat; Jahre daraus summiert. Liste auf dem Heap, der Web-Task-Stack ist klein
    Bar *list = static_cast<Bar *>(heap_caps_malloc(sizeof(Bar) * (MAX_MONTHS + 1), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!list) list = static_cast<Bar *>(malloc(sizeof(Bar) * (MAX_MONTHS + 1))); // Board ohne PSRAM
    if (!list) return;
    int n = 0;
    for (int i = 0; i < st->nMonths; i++) list[n++] = st->months[i];
    if (haveToday) {
      const uint32_t mk = td.key / 100;
      if (n && list[n - 1].key == mk) {
        Bar &m = list[n - 1];
        m.pv += td.pv; m.imp += td.imp; m.exp += td.exp; m.chg += td.chg; m.dis += td.dis; m.house += td.house;
      } else {
        list[n++] = Bar{mk, td.pv, td.imp, td.exp, td.chg, td.dis, td.house};
      }
    }
    if (!strcmp(unit, "month")) {
      for (int i = max(0, n - count); i < n; i++) {
        keyToStr(list[i].key, true, label, sizeof(label));
        putBar(arr, label, list[i]);
      }
    } else {
      Bar y{};
      for (int i = 0; i <= n; i++) {
        const bool flush = i == n || (y.key && list[i].key / 100 != y.key);
        if (flush && y.key) {
          snprintf(label, sizeof(label), "%lu", (unsigned long)y.key);
          putBar(arr, label, y);
          y = Bar{};
        }
        if (i == n) break;
        if (!y.key) y.key = list[i].key / 100;
        y.pv += list[i].pv; y.imp += list[i].imp; y.exp += list[i].exp;
        y.chg += list[i].chg; y.dis += list[i].dis; y.house += list[i].house;
      }
    }
    heap_caps_free(list);
  }
  out["since"] = st->nDays ? st->days[0].key : st->dayKey;
}

void downloadCsv(AsyncWebServerRequest *req) {
  struct Snapshot {
    Store data{};
    Bar current{};
    Sample running{};
    uint32_t nowSlot = 0;
    bool haveRunning = false;
    double pv, imp, exp;
  };
  auto snap = std::shared_ptr<Snapshot>(new (std::nothrow) Snapshot);
  if (!snap) {
    req->send(503, "text/plain; charset=utf-8",
              ConfigStore::uiText("Nicht genug Speicher für den Download.\n", "Not enough memory for the download.\n"));
    return;
  }
  {
    Guard guard;
    if (st) {
      snap->data = *st;
      if (st->dayKey) snap->current = today();
    }
    if (timeValid()) snap->nowSlot = uint32_t(time(nullptr)) / SLOT_S;
    snap->haveRunning = sumN && curSlot == snap->nowSlot;
    if (snap->haveRunning) {
      snap->running = Sample{clamp16(sumPv / int32_t(sumN)), clamp16(sumBat / int32_t(sumN)),
                             clamp16(sumGrid / int32_t(sumN)), clamp16(sumHouse / int32_t(sumN))};
    }
    snap->pv = gLive.energyPvKwh;
    snap->imp = gLive.energyImpKwh;
    snap->exp = gLive.energyExpKwh;
  }
  auto *response = req->beginChunkedResponse("text/csv; charset=utf-8",
      [snap](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t written = 0;
        const Store &s = snap->data;
        const size_t dayCount = s.nDays + (snap->current.key ? 1 : 0);
        const bool extraMonth = snap->current.key &&
            (!s.nMonths || s.months[s.nMonths - 1].key != snap->current.key / 100);
        const size_t monthCount = s.nMonths + (extraMonth ? 1 : 0);
        const size_t slotCount = snap->nowSlot ? SLOTS : 0;
        char line[256];
        for (size_t row = 0; row < 2 + slotCount + dayCount + monthCount && written < maxLen; row++) {
          if (!row) {
            strlcpy(line, "Typ,Zeit,PV_W,Akku_W,Netz_W,Haus_W,PV_kWh,Bezug_kWh,Einspeisung_kWh,Ladung_kWh,Entladung_kWh,Haus_kWh\r\n", sizeof(line));
          } else if (row <= slotCount) {
            const uint32_t stamp = snap->nowSlot - (SLOTS - row);
            const size_t i = stamp % SLOTS;
            const bool running = stamp == snap->nowSlot && snap->haveRunning;
            if (!running && s.slotStamp[i] != stamp) continue;
            const Sample &v = running ? snap->running : s.slots[i];
            snprintf(line, sizeof(line), "Leistung,%lu,%d,%d,%d,%d,,,,,,\r\n",
                     (unsigned long)(stamp * SLOT_S), v.pv, v.bat, v.grid, v.house);
          } else if (row <= slotCount + dayCount + monthCount) {
            const size_t i = row - slotCount - 1;
            const bool month = i >= dayCount;
            Bar b;
            if (!month) b = i < s.nDays ? s.days[i] : snap->current;
            else {
              const size_t m = i - dayCount;
              b = m < s.nMonths ? s.months[m] : Bar{snap->current.key / 100, 0, 0, 0, 0, 0, 0};
              if (snap->current.key && b.key == snap->current.key / 100) {
                b.pv += snap->current.pv; b.imp += snap->current.imp; b.exp += snap->current.exp;
                b.chg += snap->current.chg; b.dis += snap->current.dis; b.house += snap->current.house;
              }
            }
            char label[16];
            keyToStr(b.key, month, label, sizeof(label));
            snprintf(line, sizeof(line), "%s,%s,,,,,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\r\n",
                     month ? "Monat" : "Tag", label, b.pv, b.imp, b.exp, b.chg, b.dis, b.house);
          } else {
            snprintf(line, sizeof(line), "Zaehler,Gesamt,,,,,%.3f,%.3f,%.3f,,,\r\n", snap->pv, snap->imp, snap->exp);
          }
          const size_t len = strlen(line);
          if (index >= len) { index -= len; continue; }
          const size_t n = min(len - index, maxLen - written);
          memcpy(buffer + written, line + index, n);
          written += n;
          index = 0;
        }
        return written;
      });
  response->addHeader("Content-Disposition", "attachment; filename=\"hit-proxy-verlauf.csv\"");
  response->addHeader("Cache-Control", "no-store");
  req->send(response);
}

} // namespace History
