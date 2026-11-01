# T-Connect-Pro Pins (LilyGO Wiki / GitHub)

**Kein PoE** — nur 12–24 V DC oder USB. Ethernet nur Daten.

| Funktion | Pins |
|---|---|
| Display ST7796 | MOSI 11, MISO 13, SCLK 12, DC 41, CS 21, BL 46 |
| Touch CST226SE | SDA 39, SCL 40, RST 47, INT 3 |
| Ethernet W5500 | MOSI 11, MISO 13, SCLK 12, CS 10, INT 9, RST 48 |
| RS485 | TX 17, RX 18 |
| RS232 | TX 4, RX 5 |
| CAN | TX 6, RX 7 |
| LoRa SX1262 | CS 14, RST 42, DIO1 45, BUSY 38 (+ shared SPI 11/12/13) |

SPI-Bus Display/Eth/LoRa geteilt (CS getrennt).
