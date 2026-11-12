#include "EventLog.h"
#include <time.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>

namespace {
constexpr size_t N = 80;
SysEvt buf[N];
size_t head = 0;
size_t used = 0;
uint32_t warnCount = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void jsonStr(String &out, const char *s) {
  out += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') out += '\\';
    if (uint8_t(*s) >= 0x20) out += *s;
  }
  out += '"';
}
} // namespace

namespace EventLog {

void add(char level, const char *src, const char *fmt, ...) {
  SysEvt e;
  e.ms = millis();
  const time_t now = time(nullptr);
  e.epoch = now > 1700000000 ? uint32_t(now) : 0;
  e.level = level;
  strlcpy(e.src, src ? src : "", sizeof(e.src));
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(e.msg, sizeof(e.msg), fmt, ap);
  va_end(ap);
  Serial.printf("[%c] %s: %s\n", e.level, e.src, e.msg);

  portENTER_CRITICAL(&mux);
  buf[head] = e;
  head = (head + 1) % N;
  if (used < N) used++;
  if (level != 'I') warnCount++;
  portEXIT_CRITICAL(&mux);
}

void clear() {
  portENTER_CRITICAL(&mux);
  head = 0;
  used = 0;
  portEXIT_CRITICAL(&mux);
}

size_t count() { return used; }
uint32_t warnings() { return warnCount; }

String toJson(size_t maxN) {
  static SysEvt snap[N];
  size_t n, start;
  portENTER_CRITICAL(&mux);
  n = used < maxN ? used : maxN;
  start = (head + N - n) % N;
  for (size_t i = 0; i < n; i++) snap[i] = buf[(start + i) % N];
  portEXIT_CRITICAL(&mux);

  String out;
  out.reserve(n * 110 + 4);
  out += '[';
  for (size_t i = 0; i < n; i++) {
    const SysEvt &e = snap[i];
    if (i) out += ',';
    out += "{\"t\":"; out += e.ms;
    out += ",\"ts\":"; out += e.epoch;
    out += ",\"lv\":\""; out += e.level; out += '"';
    out += ",\"src\":"; jsonStr(out, e.src);
    out += ",\"msg\":"; jsonStr(out, e.msg);
    out += '}';
  }
  out += ']';
  return out;
}

} // namespace EventLog
