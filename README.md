# HIT → Consolinno SunSpec Proxy (ESP32)

Autarker Modbus-Proxy: **Hoymiles HIT** → **Consolinno HEMS** (SunSpec GEN24 auf Port **1502**).

Zielboard: [LILYGO T-Connect-Pro](https://github.com/Xinyuan-LilyGO/T-Connect-Pro) (ESP32-S3, Ethernet W5500, Display, Touch). **Kein PoE** — 12–24 V DC.

Aktuelle Firmware: **v0.3.11**

## Architektur

```
Hoymiles HIT  --Modbus TCP-->  ESP Proxy (:1502 SunSpec)
                                      |
                                      +-- optional Tasmota (Display)
                                      +-- optional MQTT → Home Assistant
                                      +-- optional KNX IP (Multicast)
                                      +-- optional HIT Write (Akku/APC)
```

| Pfad | Pflicht? | Rolle |
|------|----------|--------|
| HIT → SunSpec | ja | Consolinno HEMS |
| Tasmota | nein | Display / optional Meter-Spoof |
| MQTT | nein | HA-Anzeige + Discovery / Ingest-Fallback |
| Cloud | nein | Hoymiles S-Miles Lesen (Fallback) |
| KNX IP | nein | Multicast-Routing aller wählbaren Live-Werte |
| HIT Write | nein | Reg. 306–309 / APC (Web-Schalter) |

## Screenshots

> **Original-UI aus der Firmware** (HTML/CSS gerendert, Demo-JSON RFC‑5737) — keine KI-Mockups.  
> Display: pixelgetreue **480×222**-Vorschau aus `StatusDisplay.cpp`.

### Live-Dashboard (`/`)

![Live Dashboard](docs/screenshots/live-dashboard.jpg)

### Einstellungen · KNX

![Web Config](docs/screenshots/web-config.jpg)

![KNX Datenpunkte](docs/screenshots/web-config-knx.jpg)

### Display Live (480×222)

![Display Live](docs/screenshots/display-live.jpg)

## Features

- **Live-Dashboard** mit Sidebar-Menü (HIT / Netzwerk / SunSpec / KNX / MQTT / Tasmota / Protokoll / OTA)
- **Themes:** Mint · Ocean · Ember (Web + Display)
- **Sprache:** Deutsch / English (Web + Display)
- Karten mit Icons, MPPT-Tiles, Status-Pills
- **KNX IP:** alle Live-Punkte wählbar, GA manuell, physische Sendeadresse
- HIT Modbus TCP (MPPT, Bat, SoC/SoH, Netzphasen, Frequenz, Energien)
- SunSpec GEN24 auf **:1502** inkl. Model **123**
- Debug `/debug`, OTA `/ota`, Demo-Modus, Dual-Auth, First-Boot-Assistent

## Erste Inbetriebnahme

1. `pio run -e t-connect-pro -t upload`
2. Display: IP + Display-Code
3. Browser `http://<IP>/` → Assistent
4. Consolinno: Proxy-IP, Port **1502**, Unit **1**

Pins: [`PINS.md`](PINS.md)

## Build

```bash
cd hit-sunspec-proxy
pio run -e t-connect-pro
pio run -e t-connect-pro -t upload
```

## Privatsphäre

Nur Pseudodaten (RFC‑5737 `192.0.2.x`). Keine echten Anlagenwerte im Repo.

## Support & Donation

Lead Developer: [atomi23](https://github.com/atomi23)

<a href="https://www.paypal.me/atomi23"><img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?logo=paypal&style=for-the-badge" alt="Donate via PayPal" /></a>

## Lizenz

MIT
