# Versionsverlauf

**Deutsch** · [English](./changelog.en.md) · [Zur Übersicht](../README.md)

## Neu in v0.7.2
- **Weboberfläche bleibt erreichbar:** Hing der Webserver oder der HTTPS-Proxy, lief SunSpec weiter, die Seiten waren aber nicht mehr zu öffnen, bis man das Gerät vom Strom nahm. Ein Selbsttest fragt jetzt alle 30 s Port 80 über 127.0.0.1 ab und startet das Gerät nach rund 2 Minuten ohne Antwort neu. Hängt nur Port 443, leitet Port 80 sofort nicht mehr auf HTTPS um; nach 10 Minuten startet das Gerät ebenfalls neu. Jeder Neustart durch den Selbsttest steht mit Grund im Systemprotokoll.

<details>
<summary><b>Ältere Versionen</b> (v0.7.1 bis v0.5.6)</summary>

### v0.7.1
- **Bluetooth-LED-Strip per KNX (nur T-Connect-Pro):** Der kleinste Helligkeitswert (DPT 5.001, Rohwert 1) dimmt den Strip auf 1 %, statt ihn auszuschalten. Nur 0 schaltet aus.
- **Seite „Bluetooth“:** „Zuletzt gesendet“ zeigt jeden Befehl, den der Strip angenommen hat, auch wenn ein späterer Befehl derselben Folge scheitert. Schnelles Bedienen startet keine parallelen Abfragen mehr (3 Klicks: 10 statt 51 Anfragen an das Gerät).
- **SunSpec und Modbus-Gateway:** Sind alle Verbindungsplätze belegt, wird die am längsten inaktive Verbindung getrennt, nicht mehr immer die erste. Das konnte bisher das aktive HEMS treffen.
- **Tasmota-Zähler:** Ein langsamer oder stummer Zähler hält SunSpec und das Modbus-Gateway nicht mehr auf (bisher bis zu 3 s je Abfrage).
- **Weboberfläche:** Wird beim Build gzip-komprimiert, die Firmware ist je Board rund 105 KiB kleiner (WT32-ETH01: 88,8 % statt 94,3 % belegt).

### v0.7.0
- **Bluetooth-Einstellungen auf der Seite „Bluetooth“ (nur T-Connect-Pro):** Strip-Suche, Name, Adresse und Einschaltbefehl stehen jetzt dort, wo der Strip auch bedient wird, statt unter *Einstellungen → KNX*. Dort bleiben nur die Gruppenadressen.
- **Seite „Übersicht“:** Zeigt Hardware (Board, Chip, Speicher, Flash), Software (Firmware, Baudatum, Arduino/ESP-IDF, Laufzeit, letzter Start), Netzwerk und Dienste sowie die Auslastung: CPU-Last je Kern, Arbeitsspeicher, PSRAM und Dateisystem.
- **HTTPS trotz Bluetooth:** Seit v0.5.23 fehlten dem T-Connect-Pro rund 40 KB interner RAM für HTTPS („zu wenig RAM für HTTPS, nur HTTP“). Auf Boards mit PSRAM liegt TLS jetzt im PSRAM.

### v0.6.0
- **Seite „Bluetooth“ (nur T-Connect-Pro):** Zeigt den Verbindungszustand des LED-Strips und den letzten Fehler, schaltet den Strip ein und aus und stellt Helligkeit und Farbe ein. Angezeigt wird, was das Gateway zuletzt gesendet hat; der Strip meldet nichts zurück.
- **Bluetooth-Protokoll:** Jeder an den Strip gesendete Befehl steht mit seinen Bytes im Ereignisprotokoll, ebenso jeder Verbindungsaufbau. Fehlermeldungen nennen jetzt den Grund (nicht erreichbar oder kein passender Schreibkanal).

### v0.5.23
- **Bluetooth-LED-Strip per KNX (nur T-Connect-Pro):** LED-Strips der App *Lotus Lantern* lassen sich über drei KNX-Objekte schalten (DPT 1.001), dimmen (DPT 5.001) und färben (DPT 232.600). Einrichtung mit Strip-Suche und Test unter *Einstellungen → KNX*. Die Befehle stammen aus öffentlichen Protokollbeschreibungen und sind noch nicht an einem echten Strip erprobt.
- **Weboberfläche:** Dezimalzahlen folgen der gewählten Sprache (Komma oder Punkt).
- **HIT-Wechselrichter:** Eine abgeschnittene RS485-Antwort wird als Fehler gemeldet.
- **Datenherkunft:** Wird zurückgesetzt, wenn alle Datenquellen abgelaufen sind.

### v0.5.22
- **HTTPS unter Windows:** CA-Installationsschritte mit manueller Auswahl des Stammzertifikatsspeichers ergänzt, damit Chrome der Geräte-CA vertraut.
- **Abmelden:** Schaltfläche auf den Hauptseiten der Weboberfläche, auch über HTTPS. Falls der Browser die Anmeldung behält, hilft der Hinweis zum Schließen des Browsers.

### v0.5.20
- **Weboberfläche:** Lädt das Layout mit der Seite, statt es nach einem Neustart separat anfordern zu müssen.

### v0.5.19
- **Deutsch und Englisch:** Fehlende Texte in Sicherung und Wiederherstellung, HTTPS, Downloads und Statusmeldungen ergänzt. Neue Protokolleinträge folgen der gewählten Sprache.

### v0.5.18
- **CA-Download:** Liefert jetzt die Zertifikatsdatei statt der Zertifikatsinformationen als JSON.

### v0.5.17
- **HTTPS ohne Browserwarnung:** Die Geräte-CA signiert das Serverzertifikat; ihr CA-Zertifikat einmal als vertrauenswürdige Stamm-CA installieren.
- **CA-Zertifikat herunterladen:** Öffentlich und ohne Anmeldung unter `/api/tls/ca.crt`, auch über HTTP erreichbar.
- **Weboberfläche und OTA bleiben erreichbar:** Scheitert die CA-Umstellung, wird ein passendes gespeichertes oder ein selbst signiertes Ersatz-Zertifikat verwendet. Der nächste Start versucht die Umstellung erneut.

### v0.5.16
- **Protokolle und Verlauf:** Ereignis- und Debug-Protokoll als Textdatei, Verlaufsdaten als CSV herunterladen.
- **Display am T-Connect-Pro:** Wochenplan mit eigenen An- und Aus-Zeiten pro Wochentag. Ein optionales KNX-Objekt (DPT 1.001) schaltet die Beleuchtung ein oder aus.
- **Firmware und Reset:** Einstellungen als JSON sichern und wiederherstellen. Passwörter und Zugangscode lassen sich beim Export abwählen.

### v0.5.15
- **HIT-Abfragen:** Starten erst, wenn Netzwerk und IP-Adresse bereit sind. Dadurch landen keine netzwerkbedingten Fehlversuche beim Boot mehr in der Statistik. RS485 und Demo bleiben unverändert.
- **Abfrageintervall:** Standard jetzt 30 s. Bestehende Einstellungen bleiben erhalten; bei Bedarf unter **HIT-Wechselrichter** den Wert **Poll (ms)** auf `30000` setzen.

### v0.5.14
- Die englische Weboberfläche umfasst jetzt auch alle Einstellungs-Untermenüs, Protokolle sowie Firmware und Reset.
- Der Verlauf hat englische Diagrammbeschriftungen für Screenreader. Auf kleinen oder flachen Bildschirmen überlappen die Diagrammbereiche nicht mehr.
- Für OTA die zum Board passende Datei aus dem Release wählen. Beim **T-Connect-Pro mit Display** ist es `firmware.bin`, nicht `firmware-t-poe-pro.bin`.

### v0.5.13
- **Neuer Einrichtungsassistent** mit zwei Wegen: *Einfach* stellt Fragen („Hast du ein Consolinno HEMS?“,
  „Hast du KNX im Haus?“ …) und schaltet die Dienste daraus selbst; *Experte* zeigt alle Felder (Ports,
  Unit-IDs, RS485). Zugangsdaten für Hoymiles-Cloud, Dyness-Cloud, MQTT, Tasmota und Heim-WLAN werden gleich
  im Assistenten eingegeben; die beiden Cloud-Zugänge lassen sich dort testen. Deutsch und Englisch
- **Boards ohne Display:** Bis zur Einrichtung gilt das Standard-Passwort `hitproxy1` (Geräte-WLAN und
  Anmeldung mit `admin`). Der Assistent verlangt ein eigenes Passwort mit mindestens 8 Zeichen; danach ist
  das Standard-Passwort überall ungültig, auch das Geräte-WLAN nutzt dann das eigene Passwort
- **Boards mit Display** wie bisher: zufälliger Zugangscode und QR-Code auf dem Display
- Bereits eingerichtete Geräte behalten beim Update ihren bisherigen Zugangscode

### v0.5.12
- **Hoymiles-Cloud ohne Anlagen-ID:** Bleibt das Feld leer, nimmt der Proxy die erste Anlage des Kontos und
  schreibt ihre Nummer ins Protokoll. Bisher passierte ohne ID nichts, auch keine Meldung
- **Dyness-Cloud auf Boards ohne PSRAM:** Die Leistungsabfrage liefert über 50 KB und scheiterte am Speicher;
  sie wird jetzt Eintrag für Eintrag gelesen. Antworten, die in Stücken ankommen, brechen nicht mehr ab
- Akkuseite: Seriennummern und Fehlertext kamen als Zeichensalat an
- Akku-Cloud: nach einem Verbindungsfehler wird nach 30 s neu versucht statt erst nach dem vollen Intervall

### v0.5.11
- **Geräte-WLAN folgt dem Schalter:** Ist *Geräte-WLAN zusätzlich anlassen* aus und das Gerät hängt am Kabel,
  bleibt das WLAN jetzt wirklich aus (bisher lief es ohne Heim-WLAN immer). Als Notzugang geht es von selbst
  an, wenn das Gerät am Kabel eine Minute lang keine Adresse hat, und wieder aus, wenn das Kabel zurück ist
- **HTTPS nur mit genug Speicher:** Die Prüfung beim Start rechnet jetzt auch den Platz für parallele
  Verbindungen eines Browsers ein. Der **WT32-ETH01** (ohne PSRAM) läuft damit immer über HTTP, auch mit
  abgeschaltetem WLAN; Boards mit PSRAM nutzen weiter HTTPS

### v0.5.10
- **Boards ohne PSRAM (WT32-ETH01):** Die Einstellungsseite kam leer an, und HTTP leitete auf ein HTTPS um, das
  aus Speichermangel jede Verbindung abbrach; die Oberfläche war damit nicht erreichbar. Seiten werden jetzt
  direkt aus dem Flash gesendet. Reicht der RAM nicht für HTTPS, startet es nicht und die Oberfläche läuft
  über HTTP (steht unter *Netzwerk und Zugang* und im Protokoll)
- Diagnose-Ausgaben auf der seriellen Schnittstelle nur noch mit dem Build-Schalter `-DTLS_DIAG=1`

### v0.5.9
- Neues Design **Blaupause** mit eigener Bedienung: Menüleiste oben (am Handy unten), Einstellungen als
  aufklappendes Blatt, auf der Live-Seite ein Energieband statt des Knotenbilds. Die Bandbreite entspricht
  der Leistung; PV deckt dabei zuerst das Haus, dann den Akku, dann die Einspeisung
- Neues Design **Stahl**: hell mit dunkler Seitenleiste
- **Einrichtung am Handy:** Im Geräte-WLAN öffnet sich die Einrichtungsseite von selbst (Captive Portal)
- Netzleistung auf der Live-Seite ist auch in hellen Designs lesbar
- Neue Build-Variante **`p4-poe-eth`** für das [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm):
  Ethernet mit PoE, WLAN über den ESP32-C6; noch nicht auf Hardware getestet
- Druckbare Hutschienengehäuse ohne Schrauben (Clip angedruckt, Deckel rastet ein):
  [WT32-ETH01, 2 TE](../hardware/wt32-din/) als Variante nur PoE oder mit RS485/Step-down,
  [ESP32-P4-POE-ETH, 3,5 TE](../hardware/p4-poe-din/)
- **Akkudaten** unter *Datenquellen, Akku*: Zelltemperaturen, Zellspannungen, Lade- und Entladegrenzen und
  Pack-Infos vom HIT, dazu optional SoC, SoH, Zyklen und Restenergie aus der **Dyness-Cloud**. Die Live-Seite
  zeigt die zusammengeführten Werte, Home Assistant und KNX bekommen sie als eigene Datenpunkte.
  Dyness: App-ID und Secret im Portal [ems.dyness.com](https://ems.dyness.com) unter *Entwicklerzentrum,
  API-Verwaltung* anlegen (das Secret steht dort nur einmal) und unter *Akku* eintragen; nutzt die
  Home-Assistant-Integration dieselbe App-ID, teilen sich beide die Abrufgrenze

### v0.5.8
- **Funktionsauswahl** im Einrichtungsassistenten und unter *Einstellungen, Funktionen*: Consolinno/SunSpec,
  Modbus-Gateway, Home Assistant, KNX, Tasmota-Zähler und Hoymiles-Cloud einzeln wählbar
- Nicht gewählte Dienste starten nicht und verschwinden aus Menü und Live-Seite; SunSpec (Port 1502) abschaltbar
- Wechselrichter-Typ als Einstellung vorbereitet (bisher nur Hoymiles HIT)
- Update per OTA von älteren Versionen: Einstellungen bleiben, kein erneuter Einrichtungsassistent

### v0.5.7
- **KNX:** Zuordnungstabelle nach Bereichen gruppiert, Zeilen hinterlegt, ungültige oder doppelte Adressen rot;
  **Import/Export der Gruppenadressen im ETS-Format** (XML und CSV)
- Neue Build-Variante **`wt32-eth01`**; `t-poe-pro` korrigiert (Takt, PHY-Reset, RS485-Pins, 16 MB Flash, PSRAM laut LilyGO)
- Schutz gegen CSRF: fremde Webseiten können über ein gespeichertes Login nichts mehr auslösen
- OTA-Update gehört genau einer Anfrage; Login wird vor dem Annehmen großer Anfragen geprüft
- Home Assistant: keine Zählerstände 0 mehr vor den ersten Daten (verhinderte doppelt gezählte Energie)
- Cloud: Zählerstände in Wh werden immer richtig umgerechnet, auch unter 10 kWh
- „Jetzt an HIT schreiben“ schreibt die gerade gespeicherten Werte; „HIT testen“ im Setup wartet auf eine frische Abfrage
- Tasmota: Absturz bei numerischer Zählernummer behoben, Zugangsdaten URL-kodiert
- Verlauf übersteht einen Stromausfall beim Speichern; KNX-Typnummern für Strom/Frequenz korrigiert (14.019/14.033)
- Ungültige Eingaben (Port 0, Unit-ID 0, leerer WLAN-Name …) werden nicht mehr gespeichert

### v0.5.6
- HIT-Poll in eigener FreeRTOS-Task (UI/Consolinno bleiben responsiv)
- Tasmota wählbar als Live-Netzzähler; optional SunSpec **Model 203** (3ph-Spoof aus Summe + HIT-Spannungen)
- KNX DPT9 korrekt skaliert; FC16 Power-Limit erst nach vollständigem Registersatz
- Web-Callbacks (Poll, HIT-Schreiben, Tasmota-Probe) laufen in der Hauptschleife, nicht im Async-Task
- Passwörter werden in `/api/config` nicht mehr im Klartext ausgeliefert
- `millis()`-Rollover-fest; Platform `espressif32 @ 53.3.13` gepinnt; eigene 16‑MB-Partitionstabelle

</details>
