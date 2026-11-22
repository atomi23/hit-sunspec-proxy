# Integrationen

**Deutsch** · [English](./integrations.en.md) · [Zur Übersicht](../README.md)

## Architektur

```
                         ┌── SunSpec :1502 (optional) ──→ Consolinno HEMS
Hoymiles HIT ──TCP/RS485─┤── Modbus-Gateway :502 ──→ Home Assistant, andere Modbus-Clients
   (eine Verbindung)     ├── MQTT + Discovery ──→ Home Assistant
          ESP32          ├── KNX IP Routing ──→ KNX-Bus
                         └── Display, Web-UI, optional Tasmota-Zähler
```

| Pfad | Pflicht? | Rolle |
|------|----------|--------|
| HIT (Modbus TCP oder RS485) | für direkten HIT-Zugriff | Primäre Datenquelle, einzige Verbindung zum HIT; für Live-Daten sind MQTT und Hoymiles-Cloud als Ersatzquellen möglich |
| SunSpec :1502 | nur für Consolinno | Standardport für die GEN24-Emulation (Modelle 1/103/120–124/160; optional Meter 203); Dienst abschaltbar |
| Modbus-Gateway | nein | Andere Modbus-Clients lesen den HIT über den ESP |
| MQTT | nein | HA-Sensoren per Discovery, optional MQTT als Ersatzquelle |
| KNX IP | nein | Multicast-Routing wählbarer Live-Werte; empfängt auf dem T-Connect-Pro auch Schaltbefehle für Display und LED-Strip |
| Tasmota | nein | Netzzähler für Live-View und optional SunSpec Model 203 |
| Hoymiles-Cloud | nein | S-Miles nur lesen, Ersatzquelle wenn HIT und MQTT ausfallen |
| HIT-Schreiben | nein | Akku-Grenzen 306–309, APC 279/280, standardmäßig aus |

<a id="bluetooth"></a>

## Bluetooth-LED-Strip per Weboberfläche und KNX (T-Connect-Pro)

LED-Strips, die mit der App **Lotus Lantern** laufen (Bluetooth-Namen `ELK-BLEDOM…`, `MELK-…`, `LEDBLE…`), lassen
sich über KNX schalten. Unter **Einstellungen → KNX** den Strip suchen, auswählen, speichern und mit **Test: an/aus**
prüfen. Danach den Empfangsobjekten Gruppenadressen geben: **Schalten** (DPT 1.001), **Helligkeit** (DPT 5.001) und
**Farbe RGB** (DPT 232.600). Der ESP verbindet sich nur zum Senden und gibt den Strip nach 10 Sekunden wieder frei,
weil der Strip nur eine Bluetooth-Verbindung annimmt; die App am Handy muss dafür geschlossen sein. Das gibt es nur
auf dem T-Connect-Pro (ESP32-S3): auf den ESP32-Boards reicht der Speicher für Bluetooth nicht, der ESP32-P4 hat kein
Bluetooth. Der Befehlssatz stammt aus öffentlichen Protokollbeschreibungen (ELK-BLEDOM).

Auch ohne KNX-Gruppenadressen lässt sich der eingerichtete Strip auf der Seite **Bluetooth** schalten,
dimmen und färben. Die Seite zeigt den Verbindungszustand, den letzten Fehler, die zuletzt gesendeten
Werte und die LED-Einträge des Ereignisprotokolls. Die gesendeten Werte sind keine Rückmeldung des Strips.
