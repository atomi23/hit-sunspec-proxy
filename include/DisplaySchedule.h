#pragma once
#include <stdint.h>

struct DisplayDay {
  bool enabled = true;
  uint16_t start = 7 * 60;
  uint16_t end = 22 * 60;

  bool operator==(const DisplayDay &other) const {
    return enabled == other.enabled && start == other.start && end == other.end;
  }
};

namespace DisplaySchedule {
constexpr int DAYS = 7;
constexpr int MINUTES_PER_DAY = 24 * 60;
enum class Mode : uint8_t { Automatic, Off, On };

// Wochenplan: Montag = 0. Ein Nachtfenster gehört zum Tag, an dem es beginnt.
inline bool isOn(const DisplayDay (&week)[DAYS], int day, int minute) {
  const auto &today = week[day];
  const auto &yesterday = week[(day + DAYS - 1) % DAYS];
  return (today.enabled &&
          (today.start == today.end ||
           (minute >= today.start && (today.start > today.end || minute < today.end)))) ||
         (yesterday.enabled && yesterday.start > yesterday.end && minute < yesterday.end);
}

class Control {
 public:
  Mode update(Mode next) {
    if (next != scheduled) {
      // Die erste Zeit-Synchronisierung ist noch kein Ablauf eines Zeitfensters.
      if (scheduled != Mode::Automatic) clearOverride();
      scheduled = next;
    }
    return overrideMode == Mode::Automatic ? scheduled : overrideMode;
  }

  void setOverride(bool on) { overrideMode = on ? Mode::On : Mode::Off; }
  void clearOverride() { overrideMode = Mode::Automatic; }

 private:
  Mode scheduled = Mode::Automatic;
  Mode overrideMode = Mode::Automatic;
};
}
