#pragma once
#include <Arduino.h>

enum class UiLang : uint8_t { De = 0, En = 1 };
enum class UiTheme : uint8_t { Mint = 0, Ocean = 1, Ember = 2 };

inline const char *uiLangCode(UiLang l) { return l == UiLang::En ? "en" : "de"; }
inline const char *uiThemeCode(UiTheme t) {
  switch (t) {
    case UiTheme::Ocean: return "ocean";
    case UiTheme::Ember: return "ember";
    default: return "mint";
  }
}
inline UiLang uiLangFrom(const char *s) {
  if (s && (s[0] == 'e' || s[0] == 'E')) return UiLang::En;
  return UiLang::De;
}
inline UiTheme uiThemeFrom(const char *s) {
  if (!s) return UiTheme::Mint;
  if (!strcmp(s, "ocean")) return UiTheme::Ocean;
  if (!strcmp(s, "ember")) return UiTheme::Ember;
  return UiTheme::Mint;
}

// Compact display strings: index by UiLang
struct DispStr {
  const char *de;
  const char *en;
  const char *get(UiLang l) const { return l == UiLang::En ? en : de; }
};

namespace DispI18n {
inline const DispStr &pv() {
  static const DispStr s{"PV LEISTUNG", "PV POWER"};
  return s;
}
inline const DispStr &bat() {
  static const DispStr s{"BATTERIE", "BATTERY"};
  return s;
}
inline const DispStr &sys() {
  static const DispStr s{"SYSTEM", "SYSTEM"};
  return s;
}
inline const DispStr &yield() {
  static const DispStr s{"Erzeugung", "Yield"};
  return s;
}
inline const DispStr &running() {
  static const DispStr s{"Läuft", "Running"};
  return s;
}
inline const DispStr &fault() {
  static const DispStr s{"Fehler", "Fault"};
  return s;
}
inline const DispStr &tapNext() {
  static const DispStr s{"Tippen: nächste Seite", "Tap: next page"};
  return s;
}
inline const DispStr &updated() {
  static const DispStr s{"aktualisiert vor", "updated"};
  return s;
}
inline const DispStr &flow() {
  static const DispStr s{"Fluss", "Flow"};
  return s;
}
inline const DispStr &meter() {
  static const DispStr s{"Zähler", "Meter"};
  return s;
}
inline const DispStr &house() {
  static const DispStr s{"HAUS", "HOME"};
  return s;
}
inline const DispStr &grid() {
  static const DispStr s{"NETZ", "GRID"};
  return s;
}
inline const DispStr &importW() {
  static const DispStr s{"Bezug", "Import"};
  return s;
}
inline const DispStr &exportW() {
  static const DispStr s{"Einspeisung", "Export"};
  return s;
}
inline const DispStr &setupTitle() {
  static const DispStr s{"Setup / First Boot", "Setup / First Boot"};
  return s;
}
} // namespace DispI18n
