# Hardware and build

[Deutsch](./hardware.md) · **English** · [Back to overview](../README.en.md)

## Build

```bash
cd hit-sunspec-proxy
pio run -e t-connect-pro
pio run -e t-connect-pro -t upload
```

Platform and partitions are fixed in the repo (`espressif32 @ 53.3.13`, `partitions_16MB.csv`:
2× ~4 MB OTA + ~8 MB LittleFS). After changing the partition table, flash once via USB;
OTA between app slots remains possible afterwards.

| Env | Board | Notes |
|-----|-------|-------|
| `t-connect-pro` (default) | LILYGO T-Connect-Pro, ESP32-S3, 16 MB | Display, touch, W5500, RS485 TX 17 / RX 18 |
| `wt32-eth01` | WT32-ETH01, ESP32, 4 MB | no display; no PSRAM, so the UI runs over HTTP instead of HTTPS; LAN8720 (PHY 1, clock GPIO0, enable GPIO16); RS485 via an external converter with auto direction on IO17 (TX) / IO5 (RX) |
| `t-poe-pro` | [LILYGO T-POE-Pro](https://lilygo.cc/products/t-poe-pro), ESP32-WROVER-E, 16 MB, 8 MB PSRAM | no display; LAN8720 with PoE; RS485 on the board (TX 32 / RX 33) |
| `p4-poe-eth` | [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm), ESP32-P4, 32 MB, 32 MB PSRAM | no display; IP101 with PoE (MDC 31, MDIO 52, reset 51); Wi-Fi via the ESP32-C6; RS485 via an external converter on GPIO4 (TX) / GPIO5 (RX). Not yet tested on hardware |

### Where to buy

- [Search Amazon.de for LILYGO T-Connect-Pro (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=LILYGO+T-Connect-Pro&rh=p_85%3A20943776031)\*
- [Search Amazon.de for LILYGO T-POE-Pro (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=LILYGO+T-POE-Pro&rh=p_85%3A20943776031)\*
- [LILYGO T-POE-Pro on Amazon.de (alternative listing)](https://www.amazon.de/dp/B0CR3JMCVX?tag=atomi23-21)\* — approx. €14 shipping.
- [WT32-ETH01 on Amazon.de](https://www.amazon.de/dp/B0CW3DDWZ4?tag=atomi23-21)\*
- [Waveshare ESP32-P4-WIFI6-POE-ETH on Amazon.de](https://www.amazon.de/dp/B0G62SNY9S?tag=atomi23-21)\*
- [Search Amazon.de for Lotus Lantern Bluetooth LED strips (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=Lotus+Lantern+Bluetooth+LED+Strip&rh=p_85%3A20943776031)\* — check that the listing names the **Lotus Lantern** app before buying; individual models have not been tested.
- [Search for Lotus Lantern Bluetooth RGB strip controllers](https://www.amazon.de/s?k=Lotus+Lantern+Bluetooth+RGB+LED+Controller+ELK-BLEDOM&tag=atomi23-21)\* — for use with the T-Connect-Pro and an existing RGB strip. Check the Bluetooth name (e.g. `ELK-BLEDOM…`), output and voltage before buying. The gateway talks to the controller over Bluetooth; it does not drive the strip directly via GPIO.
- [Search for RS485 converter modules with 3.3 V logic and automatic direction control (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=RS485+TTL+3.3V+Modul+automatische+Richtungsumschaltung&rh=p_85%3A20943776031)\* — optional for the WT32-ETH01 and Waveshare ESP32-P4.
- [Search for USB-to-serial adapters with 3.3 V logic (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=USB+UART+TTL+3.3V+Adapter&rh=p_85%3A20943776031)\* — for flashing the WT32-ETH01.
- [Search for WT32-ETH01 PoE adapter boards (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=WT32-ETH01+PoE+Adapterplatine&rh=p_85%3A20943776031)\* — check the connection and enclosure dimensions before buying.
- [Search for step-down modules with 12–24 V input and 5 V output (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=DC+DC+Step+Down+12V+24V+auf+5V+Modul&rh=p_85%3A20943776031)\* — for the WT32-ETH01 RS485 enclosure variant; check the input voltage range and dimensions.
- [Search for Tasmota optical readers for electricity meters (Prime filter)](https://www.amazon.de/s?tag=atomi23-21&k=Tasmota+Lesekopf+Stromz%C3%A4hler&rh=p_85%3A20943776031)\* — check compatibility with your meter before buying.

**PoE and wiring**

- [Search for active IEEE PoE injectors with a power supply](https://www.amazon.de/s?k=aktiver+IEEE+PoE+Injektor+mit+Netzteil&tag=atomi23-21)\* — for setups without a PoE switch. Match the PoE standard and power requirements to the manufacturer's specifications for the T-POE-Pro, Waveshare board or separate WT32-ETH01 adapter board. Do not connect passive PoE unless explicitly supported.
- [Search for Cat 5e RJ45 patch cables](https://www.amazon.de/s?k=RJ45+Cat+5e+Ethernet+Patchkabel&tag=atomi23-21)\* — for wired Ethernet, including between the PoE source and the board.
- [Search for USB data cables](https://www.amazon.de/s?k=USB+Datenkabel+zur+Daten%C3%BCbertragung&tag=atomi23-21)\* — for flashing over USB, e.g. on the T-Connect-Pro. Choose connectors that match the board and computer, not a charge-only cable.
- [Search for Dupont jumper wires](https://www.amazon.de/s?k=Dupont+Jumperkabel+Stecker+Buchse&tag=atomi23-21)\* — for temporarily connecting a USB-to-serial adapter to the WT32-ETH01; choose connectors that fit the pin headers.

\*As an Amazon Associate I earn from qualifying purchases. Links marked * are affiliate/advertising links.

The WT32-ETH01 uses `min_spiffs.csv` (2× 1.9 MB OTA), the T-POE-Pro uses `partitions_16MB.csv` like the T-Connect-Pro.
The WT32-ETH01 is flashed via a USB-serial adapter on TX0/RX0 (IO0 to GND at power-on):

```bash
pio run -e wt32-eth01 -t upload
```

[See the board pinout](../PINS.md).
