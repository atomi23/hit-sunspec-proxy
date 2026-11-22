#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Befehle der LED-Controller, die die App "Lotus Lantern" bedient (ELK-BLEDOM und Verwandte).
// Quellen: github.com/FergusInLondon/ELK-BLEDOM (PROTCOL.md), github.com/dave-code-ruiz/elkbledom (models.json)
namespace LotusLantern {
constexpr size_t CMD_LEN = 9;

// Die Controller unterscheiden sich im Einschaltbefehl.
enum class Variant : uint8_t { Auto = 0, F0 = 1, FF = 2, One = 3 };

struct Model {
  const char *prefix;
  Variant variant;
  bool ffe1; // Schreib-Characteristic FFE1 (Dienst FFE0) statt FFF3 (Dienst FFF0)
};

inline const Model *models(size_t &n) {
  static const Model k[] = {
      {"ELK-BLEDOM0E", Variant::FF, false}, {"ELK-BLEDOB", Variant::FF, false}, {"MELK-OG10", Variant::FF, false},
      {"MELK-O", Variant::F0, false},       {"MELK", Variant::One, false},      {"ELK-BULB2", Variant::F0, false},
      {"ELK-BULB", Variant::One, false},    {"ELK-LAMPL", Variant::One, false}, {"ELK-", Variant::F0, false},
      {"LEDBLE", Variant::One, true},
  };
  n = sizeof(k) / sizeof(k[0]);
  return k;
}

// Längster passender Namensanfang; nullptr = unbekanntes Gerät.
inline const Model *modelFor(const char *name) {
  size_t n, best = 0;
  const Model *k = models(n), *hit = nullptr;
  if (!name) return nullptr;
  for (size_t i = 0; i < n; i++) {
    const size_t len = strlen(k[i].prefix);
    if (len > best && strncmp(name, k[i].prefix, len) == 0) {
      best = len;
      hit = &k[i];
    }
  }
  return hit;
}

inline Variant variantFor(const char *name, Variant chosen) {
  if (chosen != Variant::Auto) return chosen;
  const Model *m = modelFor(name);
  return m ? m->variant : Variant::F0;
}

inline void fill(uint8_t *o, uint8_t b1, uint8_t cmd, uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t b7) {
  const uint8_t f[CMD_LEN] = {0x7e, b1, cmd, p0, p1, p2, p3, b7, 0xef};
  memcpy(o, f, CMD_LEN);
}

inline void power(uint8_t *o, Variant v, bool on) {
  if (v == Variant::FF) fill(o, 7, 4, on ? 0xff : 0, 0, on ? 1 : 0, 2, 1);
  else if (v == Variant::One) on ? fill(o, 0, 4, 1, 0, 0, 0, 0) : fill(o, 0, 4, 0, 0, 0, 0xff, 0);
  else fill(o, 4, 4, on ? 0xf0 : 0, 0, on ? 1 : 0, 0xff, 0);
}

inline void brightness(uint8_t *o, uint8_t pct) { fill(o, 4, 1, pct > 100 ? 100 : pct, 0xff, 0, 0xff, 0); }

inline void color(uint8_t *o, uint8_t r, uint8_t g, uint8_t b) { fill(o, 7, 5, 3, r, g, b, 0x10); }
}
