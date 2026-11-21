#include "DebugLog.h"
#include <freertos/FreeRTOS.h>

namespace {
constexpr size_t N = 48;
DebugEvt buf[N];
size_t head = 0; // next write
size_t used = 0;
uint32_t lastMs = 0;
uint16_t lastA = 0, lastV = 0;
char lastN[40] = "";
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
}

namespace DebugLog {

void clear() {
  portENTER_CRITICAL(&mux);
  head = 0;
  used = 0;
  lastMs = 0;
  lastA = lastV = 0;
  lastN[0] = '\0';
  portEXIT_CRITICAL(&mux);
}

void add(uint8_t fc, uint16_t addr, uint16_t value, const char *note) {
  portENTER_CRITICAL(&mux);
  DebugEvt &e = buf[head];
  e.ms = millis();
  e.fc = fc;
  e.addr = addr;
  e.value = value;
  strlcpy(e.note, note ? note : "", sizeof(e.note));
  head = (head + 1) % N;
  if (used < N) used++;
  lastMs = e.ms;
  lastA = addr;
  lastV = value;
  strlcpy(lastN, e.note, sizeof(lastN));
  portEXIT_CRITICAL(&mux);
}

size_t count() { return used; }

size_t copyTo(DebugEvt *out, size_t capacity) {
  portENTER_CRITICAL(&mux);
  const size_t n = used < capacity ? used : capacity;
  const size_t start = (head + N - used) % N;
  for (size_t i = 0; i < n; i++) out[i] = buf[(start + i) % N];
  portEXIT_CRITICAL(&mux);
  return n;
}

const DebugEvt *at(size_t i) {
  if (i >= used) return nullptr;
  size_t start = (head + N - used) % N;
  return &buf[(start + i) % N];
}

String toJson(size_t maxN) {
  size_t n = used;
  if (n > maxN) n = maxN;
  size_t startIdx = used > maxN ? used - maxN : 0;
  String out = "[";
  for (size_t i = 0; i < n; i++) {
    const DebugEvt *e = at(startIdx + i);
    if (!e) continue;
    if (i) out += ',';
    out += "{\"t\":";
    out += e->ms;
    out += ",\"fc\":";
    out += e->fc;
    out += ",\"addr\":";
    out += e->addr;
    out += ",\"val\":";
    out += e->value;
    out += ",\"note\":\"";
    out += e->note;
    out += "\"}";
  }
  out += ']';
  return out;
}

uint32_t lastWriteMs() { return lastMs; }
uint16_t lastAddr() { return lastA; }
uint16_t lastValue() { return lastV; }
const char *lastNote() { return lastN; }

} // namespace DebugLog
