# HIT → Consolinno SunSpec Proxy (ESP32)

**Deutsch** · [English](./README.en.md)

Autarker Modbus-Proxy: **Hoymiles HIT** → **Consolinno HEMS** (SunSpec GEN24 auf Port **1502**).
Der ESP hält die einzige HIT-Verbindung und verteilt die Daten an Consolinno, Home Assistant, KNX und
andere Modbus-Clients. Welche dieser Dienste laufen, wählst du im Einrichtungsassistenten; der Proxy
lässt sich auch ganz ohne Consolinno nutzen, z. B. nur als Modbus-Gateway mit KNX oder Home Assistant.

Zielboard: [LILYGO T-Connect-Pro](https://github.com/Xinyuan-LilyGO/T-Connect-Pro) (ESP32-S3, Ethernet W5500, Display, Touch, RS485). **Kein PoE**, 12–24 V DC.

Ohne Display auch auf **WT32-ETH01** (ESP32 + LAN8720, 4 MB Flash) und LilyGO T-POE-Pro, siehe [Build](docs/hardware.md#build).
Für den WT32-ETH01 gibt es ein [Hutschienengehäuse (2 TE) zum Drucken](hardware/wt32-din/), für den Waveshare ESP32-P4-WIFI6-POE-ETH (`p4-poe-eth`) eines mit [3,5 TE](hardware/p4-poe-din/).

Aktuelle Firmware: **v0.7.0** — [Release mit Firmware für jedes Board](https://github.com/atomi23/hit-sunspec-proxy/releases/tag/v0.7.0)

## Schnelllinks

- [Inbetriebnahme](docs/setup.md#inbetriebnahme) · [Firmware bauen und flashen](docs/hardware.md#build)
- [Hardware und Verkabelung](docs/hardware.md) · [Bezugsquellen](docs/hardware.md#bezugsquellen)
- [Weboberfläche und Display](docs/web-ui.md) · [HTTPS-Zertifikat einrichten](docs/web-ui.md#https)
- [Integrationen und Datenwege](docs/integrations.md#architektur) · [Bluetooth-LED-Strip](docs/integrations.md#bluetooth)
- [Übersicht und CPU-Last](docs/web-ui.md#übersicht) · [Funktionen](#funktionen) · [Versionsverlauf](docs/changelog.md)

\*Als Amazon-Partner verdiene ich an qualifizierten Verkäufen. Mit * markierte Links sind Werbe-/Affiliate-Links.

## Funktionen

- **Funktionsauswahl** im Einrichtungsassistenten und unter *Einstellungen, Funktionen*: SunSpec für Consolinno,
  Modbus-Gateway, Home Assistant, KNX, Tasmota und Cloud einzeln wählbar; nicht gewählte Dienste starten nicht
  und verschwinden aus dem Menü
- **Akkudaten** vom HIT (Zellen, Temperaturen, Grenzen, Packs) und optional aus der Dyness-Cloud,
  zusammengeführt auf Live-Seite, Home Assistant und KNX
- **Live-Seite** mit Energiefluss (PV, Akku, HIT, Netz, Haus), Ladestand-Ring, Strings, Phasen, Zählern
- **Übersicht** mit CPU-Last je Kern, Speicherauslastung, Geräteinformationen, Netzwerk und Diensten.
- **Bluetooth-LED-Strip** (nur T-Connect-Pro): Suche, Einstellungen und Bedienung auf der Bluetooth-Seite;
  die Gruppenadressen bleiben unter *Einstellungen → KNX*.
- **Verlauf** im ESP gespeichert (LittleFS): 24 h in 5-Minuten-Schritten, Tage, Monate, Jahre
- **Zwei Protokolle:** System (HIT, Cloud, OTA, Neustarts) und §14a-Schreibzugriffe von Consolinno;
  das Display zeigt HIT-Fehler als Fehler/Abfragen
- Oberfläche nutzt die volle Browserbreite
- **Menü nach Datenweg:** Datenquellen, Ausgänge, Gerät; ein Bereich pro Menüpunkt
- **Display** 480×222 mit kantengeglätteter Barlow-Schrift, drei Seiten (Live, Strings und Netz, System),
  einstellbarer Abschalt-Timeout, wacht bei Berührung oder HIT-Störung auf
- **HIT über TCP oder RS485** (Modbus RTU, TX 17 / RX 18), eine dauerhafte Verbindung für Lesen und Schreiben
- **Modbus-Gateway** (Port 502): Lesen immer, Schreiben nur mit Freigabe, jeder Schreibzugriff im Protokoll
- **SunSpec** GEN24-kompatibel, Abbild ab Adresse 0, 40000 und 50000
- **Schutz des Netzschutz-Bereichs:** FC6 auf HIT-Register 101–299 wird nie gesendet
  (laut Hoymiles V2.8 lädt das alle Netzschutz-Werkswerte)
- **HTTPS** auf Port 443, HTTP leitet um (außer während der Einrichtung). Auf Boards ohne PSRAM reicht der
  Speicher dafür nicht; dort läuft die Oberfläche über HTTP. Auf unterstützten Boards nutzt TLS verfügbaren
  PSRAM. Von der Geräte-CA signiertes Zertifikat mit
  IP-Adresse und `<gerätename>.local`, wird bei neuer IP oder vor Ablauf selbst erneuert; eigenes Zertifikat
  (PEM, auch mit Zwischenzertifikaten) hochladbar, der ESP prüft ob Schlüssel und Zertifikat zusammenpassen
- **Werksreset**, **Neustart** (mit letztem Neustartgrund) und **OTA-Update** in der Weboberfläche
- **Sicherung und Wiederherstellung** unter *Firmware und Reset*: Einstellungen als JSON exportieren
  und importieren. Passwörter und Zugangscode sind standardmäßig im Klartext enthalten und lassen sich
  beim Export abwählen. HTTPS-Zertifikat, Verlauf und Protokolle sind nicht Teil der Sicherung.
- **Zugangscode** jederzeit neu erzeugen; bleibt sonst über Neustarts und Updates erhalten
- **Heim-WLAN** zusätzlich zum Geräte-WLAN (Notzugang), Änderungen ohne Neustart; das Geräte-WLAN lässt sich
  abschalten und kommt nur zurück, wenn das Gerät am Kabel keine Adresse bekommt
- **Live-Ansicht des Displays** in der Weboberfläche; Live-Seite bleibt stehen, automatisches Blättern optional
- Designs Mint, Ocean, Ember, Hell, Stahl und Blaupause (mit eigener Menüleiste und Energieband);
  Deutsch und Englisch (Web und Display)

## Privatsphäre

Nur Pseudodaten (RFC‑5737 `192.0.2.x`). Keine echten Anlagenwerte, Adressen oder Zugangsdaten im Repo.
Gespeicherte Passwörter (Web, MQTT, Tasmota, Cloud) werden über die API nicht ausgeliefert;
leere Felder beim Speichern lassen bestehende Werte unverändert.

## Support & Donation

Lead Developer: [atomi23](https://github.com/atomi23)

<a href="https://www.paypal.me/atomi23"><img src="https://img.shields.io/badge/Donate-PayPal-blue.svg?logo=paypal&style=for-the-badge" alt="Donate via PayPal" /></a>

## Lizenz

MIT. Schrift Barlow: SIL Open Font License 1.1 ([`docs/FONT-LICENSE-OFL.txt`](docs/FONT-LICENSE-OFL.txt)).
