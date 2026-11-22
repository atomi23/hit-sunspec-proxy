# Hardware und Build

**Deutsch** · [English](./hardware.en.md) · [Zur Übersicht](../README.md)

## Build

```bash
cd hit-sunspec-proxy
pio run -e t-connect-pro
pio run -e t-connect-pro -t upload
```

Platform und Partitionen sind im Repo festgelegt (`espressif32 @ 53.3.13`, `partitions_16MB.csv`:
2× ~4 MB OTA + ~8 MB LittleFS). Nach einem Partitionstabellen-Wechsel einmal per USB flashen;
OTA zwischen App-Slots bleibt danach möglich.

| Env | Board | Hinweise |
|-----|-------|----------|
| `t-connect-pro` (Standard) | LILYGO T-Connect-Pro, ESP32-S3, 16 MB | Display, Touch, W5500, RS485 TX 17 / RX 18 |
| `wt32-eth01` | WT32-ETH01, ESP32, 4 MB | ohne Display; ohne PSRAM, deshalb Oberfläche über HTTP statt HTTPS; LAN8720 (PHY 1, Takt GPIO0, Enable GPIO16); RS485 über externen Wandler mit Auto-Richtung an IO17 (TX) / IO5 (RX) |
| `t-poe-pro` | [LILYGO T-POE-Pro](https://lilygo.cc/products/t-poe-pro), ESP32-WROVER-E, 16 MB, 8 MB PSRAM | ohne Display; LAN8720 mit PoE; RS485 auf der Platine (TX 32 / RX 33) |
| `p4-poe-eth` | [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm), ESP32-P4, 32 MB, 32 MB PSRAM | ohne Display; IP101 mit PoE (MDC 31, MDIO 52, Reset 51); WLAN über den ESP32-C6; RS485 über externen Wandler an GPIO4 (TX) / GPIO5 (RX). Noch nicht auf Hardware getestet |

### Bezugsquellen

- [LILYGO T-Connect-Pro auf Amazon.de suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=LILYGO+T-Connect-Pro&rh=p_85%3A20943776031)\*
- [LILYGO T-POE-Pro auf Amazon.de suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=LILYGO+T-POE-Pro&rh=p_85%3A20943776031)\*
- [LILYGO T-POE-Pro auf Amazon.de (Alternativangebot)](https://www.amazon.de/dp/B0CR3JMCVX?tag=atomi23-21)\* — ca. 14 € Versand.
- [WT32-ETH01 auf Amazon.de](https://www.amazon.de/dp/B0CW3DDWZ4?tag=atomi23-21)\*
- [Waveshare ESP32-P4-WIFI6-POE-ETH auf Amazon.de](https://www.amazon.de/dp/B0G62SNY9S?tag=atomi23-21)\*
- [Bluetooth-LED-Strips mit Lotus Lantern auf Amazon.de suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=Lotus+Lantern+Bluetooth+LED+Strip&rh=p_85%3A20943776031)\* — vor dem Kauf prüfen, ob das Angebot die App **Lotus Lantern** nennt; einzelne Modelle wurden nicht getestet.
- [RS485-Wandlermodule mit 3,3-V-Logik und automatischer Richtungsumschaltung suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=RS485+TTL+3.3V+Modul+automatische+Richtungsumschaltung&rh=p_85%3A20943776031)\* — optional für WT32-ETH01 und Waveshare ESP32-P4.
- [USB-Seriell-Adapter mit 3,3-V-Logik suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=USB+UART+TTL+3.3V+Adapter&rh=p_85%3A20943776031)\* — zum Flashen des WT32-ETH01.
- [PoE-Adapterplatinen für WT32-ETH01 suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=WT32-ETH01+PoE+Adapterplatine&rh=p_85%3A20943776031)\* — vor dem Kauf Anschluss und Gehäusemaße prüfen.
- [Step-down-Module mit 12–24 V Eingang und 5 V Ausgang suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=DC+DC+Step+Down+12V+24V+auf+5V+Modul&rh=p_85%3A20943776031)\* — für die RS485-Gehäusevariante des WT32-ETH01; Eingangsspannungsbereich und Maße prüfen.
- [Tasmota-Leseköpfe für Stromzähler suchen (Prime-Filter)](https://www.amazon.de/s?tag=atomi23-21&k=Tasmota+Lesekopf+Stromz%C3%A4hler&rh=p_85%3A20943776031)\* — vor dem Kauf prüfen, ob der Lesekopf zum eigenen Zähler passt.

\*Als Amazon-Partner verdiene ich an qualifizierten Verkäufen. Mit * markierte Links sind Werbe-/Affiliate-Links.

Der WT32-ETH01 nutzt `min_spiffs.csv` (2× 1,9 MB OTA), der T-POE-Pro wie der T-Connect-Pro `partitions_16MB.csv`.
Der WT32-ETH01 wird über einen USB-Seriell-Adapter an TX0/RX0 geflasht (IO0 beim Einschalten auf GND):

```bash
pio run -e wt32-eth01 -t upload
```

[Siehe die Pinbelegung](../PINS.md).
