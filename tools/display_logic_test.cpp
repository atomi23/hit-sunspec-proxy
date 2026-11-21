#include "DisplaySchedule.h"
#include "KnxDisplayTelegram.h"
#include <assert.h>

using DisplaySchedule::isOn;
using DisplaySchedule::Mode;

static void testWeek() {
  DisplayDay week[DisplaySchedule::DAYS];
  for (auto &day : week) day.enabled = false;
  week[0] = {true, 8 * 60, 17 * 60};
  week[4] = {true, 21 * 60, 3 * 60};
  week[6] = {true, 22 * 60, 10 * 60};

  assert(!isOn(week, 0, 17 * 60));
  assert(isOn(week, 0, 17 * 60 - 1));
  assert(isOn(week, 0, 8 * 60));  // überlappende Zeitfenster stören sich nicht
  assert(isOn(week, 0, 0));      // Sonntag -> Montag
  assert(!isOn(week, 1, 10 * 60));
  assert(!isOn(week, 4, 20 * 60));
  assert(isOn(week, 4, 21 * 60));
  assert(isOn(week, 5, 3 * 60 - 1));
  assert(!isOn(week, 5, 3 * 60));

  week[1] = {true, 0, 0};
  assert(isOn(week, 1, 0));
  assert(isOn(week, 1, 1439));
  assert(!isOn(week, 2, 0));
}

static void testOverride() {
  DisplaySchedule::Control control;
  assert(control.update(Mode::Automatic) == Mode::Automatic);
  control.setOverride(false);
  assert(control.update(Mode::Automatic) == Mode::Off);
  assert(control.update(Mode::On) == Mode::Off);   // erster NTP-Abgleich
  assert(control.update(Mode::On) == Mode::Off);   // im gleichen Zeitfenster
  control.setOverride(true);
  assert(control.update(Mode::Off) == Mode::Off);
  control.setOverride(true);
  assert(control.update(Mode::Off) == Mode::On);
  control.clearOverride();
  assert(control.update(Mode::Off) == Mode::Off);
  assert(control.update(Mode::On) == Mode::On);
}

static void testTelegram() {
  constexpr uint16_t ga = 0x0a05, ownPa = 0xfffa;
  uint8_t p[] = {6, 0x10, 5, 0x30, 0, 17, 0x29, 0, 0xbc, 0xe0, 0x11, 0x03, 0x0a, 0x05, 1, 0, 0x81};
  bool on = false;
  auto parse = [&] { return KnxDisplayTelegram::readSwitch(p, sizeof(p), ga, ownPa, on); };
  assert(parse() && on);
  p[16] = 0x80;
  assert(parse() && !on);
  p[16] = 0x40; // GroupValueResponse
  assert(!parse());
  p[16] = 0x82; // kein Ein-Bit-Wert
  assert(!parse());
  p[16] = 0x81;
  p[9] = 0x60;  // keine Gruppenadresse
  assert(!parse());
  p[9] = 0xe0;
  p[13]++;
  assert(!parse());
  p[13]--;
  assert(!KnxDisplayTelegram::readSwitch(p, sizeof(p), ga, 0x1103, on));
  assert(!KnxDisplayTelegram::readSwitch(p, sizeof(p) - 1, ga, ownPa, on));

  uint8_t withInfo[] = {6, 0x10, 5, 0x30, 0, 21, 0x29, 4, 3, 2, 0, 0, 0xbc, 0xe0, 0x11,
                        0x03, 0x0a, 0x05, 1, 0, 0x81};
  assert(KnxDisplayTelegram::readSwitch(withInfo, sizeof(withInfo), ga, ownPa, on) && on);
  withInfo[7] = 255;
  assert(!KnxDisplayTelegram::readSwitch(withInfo, sizeof(withInfo), ga, ownPa, on));
}

int main() {
  testWeek();
  testOverride();
  testTelegram();
}
