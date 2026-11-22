#include "DisplaySchedule.h"
#include "KnxDisplayTelegram.h"
#include "LotusLantern.h"
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

static void testGroupWrite() {
  constexpr uint16_t ownPa = 0xfffa;
  uint16_t ga = 0;
  uint8_t value = 0, n = 9;
  const uint8_t *data = nullptr;
  uint8_t sw[] = {6, 0x10, 5, 0x30, 0, 17, 0x29, 0, 0xbc, 0xe0, 0x11, 0x03, 0x0a, 0x05, 1, 0, 0x81};
  assert(KnxDisplayTelegram::readWrite(sw, sizeof(sw), ownPa, ga, value, data, n));
  assert(ga == 0x0a05 && value == 1 && n == 0);
  sw[16] = 0x41; // GroupValueResponse
  assert(!KnxDisplayTelegram::readWrite(sw, sizeof(sw), ownPa, ga, value, data, n));

  uint8_t pct[] = {6, 0x10, 5, 0x30, 0, 18, 0x29, 0, 0xbc, 0xe0, 0x11, 0x03, 0x0a, 0x06, 2, 0, 0x80, 0x80};
  assert(KnxDisplayTelegram::readWrite(pct, sizeof(pct), ownPa, ga, value, data, n));
  assert(ga == 0x0a06 && n == 1 && data[0] == 0x80);
  assert(!KnxDisplayTelegram::readWrite(pct, sizeof(pct), 0x1103, ga, value, data, n));

  uint8_t rgb[] = {6, 0x10, 5, 0x30, 0, 20, 0x29, 0, 0xbc, 0xe0, 0x11, 0x03, 0x0a, 0x07, 4, 0, 0x80, 1, 2, 3};
  assert(KnxDisplayTelegram::readWrite(rgb, sizeof(rgb), ownPa, ga, value, data, n));
  assert(ga == 0x0a07 && n == 3 && data[0] == 1 && data[2] == 3);
  rgb[14] = 5; // Länge passt nicht zum Paket
  assert(!KnxDisplayTelegram::readWrite(rgb, sizeof(rgb), ownPa, ga, value, data, n));
}

static void testLotusLantern() {
  using namespace LotusLantern;
  uint8_t c[CMD_LEN];
  const uint8_t on[] = {0x7e, 4, 4, 0xf0, 0, 1, 0xff, 0, 0xef}, off[] = {0x7e, 4, 4, 0, 0, 0, 0xff, 0, 0xef};
  power(c, variantFor("ELK-BLEDOM", Variant::Auto), true);
  assert(!memcmp(c, on, CMD_LEN));
  power(c, variantFor("ELK-BLEDOM", Variant::Auto), false);
  assert(!memcmp(c, off, CMD_LEN));
  assert(variantFor("MELK-OA10", Variant::Auto) == Variant::F0);
  assert(variantFor("MELK-OG10W", Variant::Auto) == Variant::FF);
  assert(variantFor("MELK-A", Variant::Auto) == Variant::One);
  assert(variantFor("ELK-BLEDOM", Variant::One) == Variant::One);
  assert(modelFor("LEDBLE-01")->ffe1 && !modelFor("Fernseher"));
  const uint8_t rgb[] = {0x7e, 7, 5, 3, 10, 20, 30, 0x10, 0xef}, bri[] = {0x7e, 4, 1, 100, 0xff, 0, 0xff, 0, 0xef};
  color(c, 10, 20, 30);
  assert(!memcmp(c, rgb, CMD_LEN));
  brightness(c, 200);
  assert(!memcmp(c, bri, CMD_LEN));
}

int main() {
  testGroupWrite();
  testLotusLantern();
  testWeek();
  testOverride();
  testTelegram();
}
