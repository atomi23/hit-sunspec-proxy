# HIT → Consolinno SunSpec Proxy (ESP32)

[Deutsch](./README.md) · **English**

Standalone Modbus proxy: **Hoymiles HIT** → **Consolinno HEMS** (SunSpec GEN24 on port **1502**).
The ESP holds the only connection to the HIT and distributes the data to Consolinno, Home Assistant, KNX and
other Modbus clients. You choose which of these services run in the setup wizard; the proxy also works
without Consolinno, e.g. only as a Modbus gateway with KNX or Home Assistant.

Target board: [LILYGO T-Connect-Pro](https://github.com/Xinyuan-LilyGO/T-Connect-Pro) (ESP32-S3, Ethernet W5500, display, touch, RS485). **No PoE**, 12–24 V DC.

Without a display it also runs on the **WT32-ETH01** (ESP32 + LAN8720, 4 MB flash) and the LilyGO T-POE-Pro, see [Build](docs/hardware.en.md#build).
There is a [printable DIN rail enclosure (2 modules)](hardware/wt32-din/) for the WT32-ETH01, and one with [3.5 modules](hardware/p4-poe-din/) for the Waveshare ESP32-P4-WIFI6-POE-ETH (`p4-poe-eth`).

Current firmware: **v0.6.0** — [release with firmware for every board](https://github.com/atomi23/hit-sunspec-proxy/releases/tag/v0.6.0)

## Quick links

- [Getting started](docs/setup.en.md#getting-started) · [Build and flash the firmware](docs/hardware.en.md#build)
- [Hardware and wiring](docs/hardware.en.md) · [Where to buy](docs/hardware.en.md#where-to-buy)
- [Web UI and display](docs/web-ui.en.md) · [Set up the HTTPS certificate](docs/web-ui.en.md#https)
- [Integrations and data paths](docs/integrations.en.md#architecture) · [Bluetooth LED strip](docs/integrations.en.md#bluetooth)
- [Features](#features) · [Release history](docs/changelog.en.md)

## Features

- **Feature selection** in the setup wizard and under *Settings, Features*: SunSpec for Consolinno,
  Modbus gateway, Home Assistant, KNX, Tasmota and cloud selectable individually; services that are not selected do not start
  and disappear from the menu
- **Battery data** from the HIT (cells, temperatures, limits, packs) and optionally from the Dyness cloud,
  merged on the live page, Home Assistant and KNX
- **Live page** with energy flow (PV, battery, HIT, grid, house), state-of-charge ring, strings, phases, meters
- **History** stored on the ESP (LittleFS): 24 h in 5-minute steps, days, months, years
- **Two logs:** system (HIT, cloud, OTA, restarts) and Consolinno's §14a write accesses;
  the display shows HIT errors as errors/polls
- UI uses the full browser width
- **Menu by data path:** data sources, outputs, device; one area per menu item
- **Display** 480×222 with anti-aliased Barlow font, three pages (Live, Strings and grid, System),
  adjustable off timeout, wakes up on touch or HIT fault
- **HIT over TCP or RS485** (Modbus RTU, TX 17 / RX 18), one persistent connection for reading and writing
- **Modbus gateway** (port 502): reading always, writing only when enabled, every write access in the log
- **SunSpec** GEN24-compatible, image at address 0, 40000 and 50000
- **Protection of the grid protection range:** FC6 to HIT registers 101–299 is never sent
  (according to Hoymiles V2.8 this loads all factory grid protection values)
- **HTTPS** on port 443, HTTP redirects (except during setup). On boards without PSRAM there is not enough
  memory for it; the UI runs over HTTP there. Certificate signed by the device CA with
  IP address and `<devicename>.local`, renewed automatically on a new IP or before expiry; a custom certificate
  (PEM, also with intermediate certificates) can be uploaded, and the ESP checks that key and certificate match
- **Factory reset**, **restart** (with the last restart reason) and **OTA update** in the web UI
- **Backup and restore** under *Firmware and reset*: export and import settings as JSON. Passwords and
  the access code are included in plain text by default and can be excluded on export. The HTTPS
  certificate, history and logs are not included.
- **Access code** can be regenerated at any time; otherwise it persists across restarts and updates
- **Home Wi-Fi** in addition to the device Wi-Fi (emergency access), changes without restart; the device Wi-Fi can be
  switched off and only comes back if the device gets no address on the cable
- **Live view of the display** in the web UI; live page stays put, automatic page cycling optional
- Designs Mint, Ocean, Ember, Light, Steel and Blueprint (with its own menu bar and energy band);
  German and English (web and display)

## Privacy

Only dummy data (RFC 5737 `192.0.2.x`). No real plant values, addresses or credentials in the repo.
Stored passwords (web, MQTT, Tasmota, cloud) are not returned by the API;
empty fields on save leave existing values unchanged.

## Support & Donation

Lead Developer: [atomi23](https://github.com/atomi23)

<a href="https://www.paypal.me/atomi23"><img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?logo=paypal&style=for-the-badge" alt="Donate via PayPal" /></a>

## License

MIT. Font Barlow: SIL Open Font License 1.1 ([`docs/FONT-LICENSE-OFL.txt`](docs/FONT-LICENSE-OFL.txt)).
