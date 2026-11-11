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

