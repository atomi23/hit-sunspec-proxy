# Inbetriebnahme und Werkzeuge

**Deutsch** · [English](./setup.en.md) · [Zur Übersicht](../README.md)

## Inbetriebnahme

1. Erstes Flashen per USB: `pio run -e t-connect-pro -t upload`\
   (oder Release-`firmware.bin` unter *Firmware und Reset* per OTA — bei hängendem OTA vorher neu starten)
2. Mit dem Geräte-WLAN `HIT-Proxy-XXXX` verbinden. Mit Display: WLAN-Name, Zugangscode und QR-Code stehen
   auf dem Display. Ohne Display: Passwort `hitproxy1`, Anmeldung mit `admin` und `hitproxy1`.
3. Die Einrichtung öffnet sich am Handy von selbst, sonst Browser `http://192.168.4.1/`. Der Assistent
   fragt ab, was gebraucht wird; ohne Display ist dabei ein eigenes Passwort Pflicht.
4. Consolinno: Gerät als Fronius Hybrid / SunSpec, **Proxy-IP**, Port **1502**, Unit-ID **1**\
   (Scan nur auf 502 findet den Proxy nicht, solange SunSpec auf 1502 lauscht)
5. Wenn HTTPS aktiv ist, läuft die Oberfläche über `https://<esp-ip>/`, sonst über HTTP (z. B. auf dem
   WT32-ETH01). Für einen HTTPS-Aufruf ohne Warnung die Geräte-CA
   unter *Netzwerk und Zugang* herunterladen und einmal als vertrauenswürdige Stamm-CA installieren
   (siehe [HTTPS](web-ui.md#https)). Alternativ ein eigenes Zertifikat hochladen.

Gibt es schon eine andere SunSpec-Quelle, lassen sich beide vergleichen:
`python tools/sunspec_check.py <alte-quelle> --compare <esp-ip>`. Andere Programme, die den HIT per Modbus
lesen, tragen danach `<esp-ip>:502` ein (Modbus-Gateway einschalten).

Pins: [`PINS.md`](../PINS.md)

## Werkzeuge

| Skript | Zweck |
|--------|-------|
| `tools/sunspec_check.py` | SunSpec-Kette lesen, zwei Geräte vergleichen (Registerversatz) |
| `tools/parity_check.py` | Eine bestehende SunSpec-Karte gegen die ESP-Umrechnung derselben HIT-Daten prüfen (nur lesend) |
| `tools/make_vlw.py` | Display-Schriften (VLW) aus Barlow erzeugen |
| `tools/embed_fonts.py` | Web-Schriften (WOFF) einbetten |
| `tools/https_check.py` | HTTPS prüfen: Umleitung, Speichern, Zertifikat hochladen/ablehnen, optional OTA |
| `tools/esp_soak.py` | Lasttest der Weboberfläche über HTTP oder HTTPS, auch mit offen gehaltener Verbindung |
| `tools/modbus_watch.py` | SunSpec-Kennung laufend lesen, langsame Antworten und Ausfälle melden |
| `tools/serial_log.py` | Seriellen Log mitschreiben |
| `tools/battery_check.py` | Akkudaten über `/api/battery` prüfen und als Tabelle zeigen; `--sign-selftest` prüft die Dyness-Signatur |

Zugangsdaten für die Werkzeuge kommen aus `ESP_USER` / `ESP_PASS`, nie aus dem Repo.
