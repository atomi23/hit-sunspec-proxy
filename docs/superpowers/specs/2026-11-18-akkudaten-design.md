# Akkudaten: zusätzliche Batteriewerte über den HIT und die Dyness-Cloud

Stand: 18.11.2026, Entwurf zur Freigabe

## Zweck

Der Proxy zeigt heute vom Akku nur SoC, SoH, Spannung, Strom und Leistung (HIT-Register 1909–1914).
Der Nutzer hat Dyness PowerBricks und möchte mehr sehen: Temperaturen, Zellspannungen, Grenzwerte,
später Zyklen und Alarme. Die Werte sollen als Übersicht in der Weboberfläche stehen und bei Bedarf
an Home Assistant (MQTT) und KNX gehen.

Zwei Quellen:

1. **HIT, herstellerunabhängig.** Der HIT bekommt die Werte vom BMS des Akkus und stellt sie in eigenen
   Registern bereit. Das funktioniert mit jedem Akku, den der HIT unterstützt, soweit das BMS die
   Werte liefert. Kein Internet nötig.
2. **Dyness-Cloud (Open API).** Mehr Details (16 Zellspannungen, Zyklen, Alarme, Restenergie), aber
   nur mit Internet und höchstens alle 5 Minuten.

## Ziele

- Akkudaten aus beiden Quellen lesen, einzeln wählbar, mit einstellbarem Intervall.
- Aufbau als Block wie beim Wechselrichter-Typ: Später kommen weitere Akku-Clouds dazu (z. B. Pylontech,
  BYD), ohne den Rest umzubauen.
- Anzeige in der Weboberfläche, Sensoren für Home Assistant, Kanäle für KNX.
- Gleicher Code auf allen Boards; nur Grenzen und Voreinstellungen hängen vom Board ab.

## Nicht-Ziele

- Keine Steuerung des Akkus über diese Funktion (Ladegrenzen bleiben bei HitControl).
- Kein lokaler Zugriff auf den Dyness-WLAN-Dongle.
- Keine Weiterleitung von Dyness per MQTT (bräuchte einen aus dem Internet erreichbaren Broker).
- Akkudaten fließen nicht in SunSpec / Consolinno und nicht in den Verlauf (History).
- Kein Ersatz der Live-Werte: Die Dyness-Cloud ist keine Fallback-Quelle für SoC oder Leistung.

## Anti-Ziele (wäre ein Fehlschlag, auch wenn es läuft)

- Der HIT-Poll oder SunSpec für Consolinno wird langsamer oder fällt aus.
- Speicherknappheit: HTTPS-Oberfläche oder Hoymiles-Cloud brechen ab, weil die Dyness-Abfrage RAM belegt.
- Bestehende KNX-Zuordnungen oder Einstellungen gehen beim Update verloren.

## Ansätze

| Ansatz | Kurz | Bewertung |
|---|---|---|
| **A: in bestehende Abläufe einhängen** | HIT-Werte in der HIT-Task, Dyness in der vorhandenen Cloud-Task | **gewählt**: kein zusätzlicher Speicher, nie zwei TLS-Verbindungen gleichzeitig |
| B: eigene Task je Quelle | eigene Tasks für Akku-HIT und Akku-Cloud | verworfen: zusätzlicher Stack, zwei TLS-Sitzungen parallel (je ca. 40 KB), auf dem WT32-ETH01 zu knapp |
| B je nach Board | A auf Boards ohne PSRAM, B auf den anderen | verworfen: doppelter Code und doppelt testen, Nutzen gering (ein Dyness-Abruf alle 5 min) |
| Dyness-MQTT-Weiterleitung | Dyness-Portal schickt an eigenen Broker | verworfen: Broker müsste aus dem Internet erreichbar sein |

Gewählt hat der Nutzer A, mit nach Board unterschiedlichen Grenzen.

## Entwurf

### Einstellungen (AppConfig, ConfigStore)

Neuer Block „Akku“, aufgebaut wie der Wechselrichter-Typ:

```cpp
// Akku-Cloud-Anbieter; weitere kommen als eigene Werte dazu
enum class BatteryCloud : uint8_t { None = 0, Dyness = 1 };
constexpr uint8_t BATTERY_CLOUD_COUNT = 2;
```

| Feld | Typ | Standard | NVS-Schlüssel |
|---|---|---|---|
| `batEnabled` | bool | true | `batena` |
| `batHitEnabled` | bool | true | `bathit` |
| `batHitPollS` | uint16 | 60 (Bereich 30–3600) | `bathitp` |
| `batCloud` | BatteryCloud | None | `batcld` |
| `batCloudHost` | char[64] | `open-api.dyness.com` | `batchost` |
| `batCloudAppId` | char[64] | "" | `batcid` |
| `batCloudSecret` | char[80] | "" (wird nie ausgeliefert) | `batcsec` |
| `batCloudSn` | char[40] | "" (leer = automatisch das erste Gerät) | `batcsn` |
| `batCloudPollS` | uint16 | 300 bzw. 600 ohne PSRAM (Untergrenze ebenso) | `batcp` |

NVS-Schlüssel höchstens 15 Zeichen. Das Secret wird wie alle Passwörter behandelt: leeres Feld im
Formular heißt unverändert, `/api/config` liefert nur `batCloudSecretSet`.

Zusätzlich `batEnabled` (bool, Standard true, NVS `batena`) als Hauptschalter.

Funktionsauswahl (Einrichtung und *Einstellungen, Funktionen*): neuer Punkt **„Akkudaten“**. Er schaltet
`batEnabled`. Ist er aus, liest keine der beiden Quellen und der Menüpunkt *Akku* verschwindet, wie bei den
anderen Diensten. Im Menüpunkt *Akku* sind die Quellen unabhängig: „vom HIT lesen“ (`batHitEnabled`) und
„Akku-Cloud“ (`batCloud`). Wer nur die Cloud will, schaltet dort die HIT-Abfrage aus. *(Entscheidung des
Entwurfs, nach Prüfung ergänzt.)*

### Daten (eigener Datensatz, nicht in gLive)

`include/BatteryData.h`:

```cpp
struct BatteryInfo {
  // HIT (Register 1916-1927, Packinfo ab 8000)
  bool hitValid; uint32_t hitAtMs;
  float maxChargeA, maxDischargeA, chargeCutV, dischargeCutV;
  int16_t modTempMax, modTempMin, cellTempMax, cellTempMin;   // °C
  float modVMax, modVMin, cellVMax, cellVMin;                  // V
  uint8_t packs; char packSn[4][33]; uint32_t ratedCapWh[4]; uint32_t bmsFw[4];
  // Cloud
  bool cloudValid; uint32_t cloudAtMs; char cloudSn[40]; char cloudErr[48];
  float cSoc, cSoh, cPowerW, cVoltV, cCurrA, cTempMax, cTempMin, cMosTemp;
  float cCellVMax, cCellVMin, cCellDiffMv, cUsableKwh, cRemainKwh;
  uint32_t cCycles; char cStatus[16]; uint32_t cAlarm;
  uint8_t cCells; float cCellV[16];      // nur Boards mit PSRAM füllen alle 16
};
```

Zugriff über eine Funktion, die unter einem Mutex eine Kopie liefert (Web-Task und Hauptschleife lesen,
HIT- und Cloud-Task schreiben). Nicht vorhandene Werte sind NAN bzw. 0 und werden nicht angezeigt
und nicht gesendet.

### Quelle 1: HIT (HitClient)

- Eigener Zähler in der HIT-Task: Nach einem erfolgreichen Haupt-Poll wird, wenn `batHitEnabled` und
  `batHitPollS` abgelaufen sind, **ein** Lesebefehl FC4 1916–1927 (12 Register) gesendet. Er belegt die
  Leitung rund 100 ms. Schlägt er fehl, wird das ins Ereignisprotokoll geschrieben (nur beim ersten
  Fehler in Folge) und der Haupt-Poll bleibt unberührt.
- Packinfo (FC3 8000–8004, je Pack 8100–8127) beim ersten erfolgreichen Poll und danach stündlich,
  höchstens 4 Packs.
- Registerbelegung laut Hoymiles „Energy Storage MODBUS Protocol V2.8“ (Input-Register, FC4, Kap. 4.2.3):

  | Reg | Wert | Typ | Faktor |
  |---|---|---|---|
  | 1916 | max. Ladestrom | U16 | A ÷10 |
  | 1917 | max. Entladestrom | U16 | A ÷10 |
  | 1918 | Ladeschlussspannung | U16 | V ÷10 |
  | 1919 | Entladeschlussspannung | U16 | V ÷10 |
  | 1920 / 1921 | Modultemperatur max / min | I16 | °C ×1 |
  | 1922 / 1923 | Zelltemperatur max / min | I16 | °C ×1 |
  | 1924 / 1925 | Modulspannung max / min | U16 | V ÷10 |
  | 1926 / 1927 | Zellspannung max / min | U16 | V ÷10 |

- Packinfo (Holding-Register, FC3, Kap. 4.1.19/4.1.20): 8000 Marke, 8001 Anzahl Cluster, 8002/8003 Anzahl
  Packs in Cluster 1/2, 8004 Meldeflag. Je Pack ein Block von 50 Registern ab 8100 (Pack 2 ab 8150, Pack 3 ab
  8200, …): +0..15 Seriennummer (String, 32 Zeichen, zwei Zeichen pro Register, High-Byte zuerst),
  +16..19 Board-SN (U64), +20/21 DC/DC-Software/Hardware (U16), +22/23 BMS-Software (U32), +26/27
  Nennkapazität (U32, Einheit nicht angegeben: als Rohwert anzeigen und am Gerät klären). Gelesen werden
  höchstens 4 Packs, je Pack ein Lesebefehl über 28 Register.
- „Nicht geliefert“: Sind alle zwölf Register 1916–1927 gleich 0, liefert das BMS nichts und der ganze Block
  gilt als fehlend. Sonst zählt jeder Wert, auch 0 °C und 0 A (ein BMS setzt 0 A bei vollem oder kaltem
  Akku). Einzeln fehlend sind nur Spannungen gleich 0 sowie 0xFFFF (U16) und 0x8000 (I16). *(Entscheidung des
  Entwurfs, nach Prüfung ergänzt.)*
- Im Demo-Modus erfundene, plausible Werte.

### Quelle 2: Dyness Open API (neues Modul `BatteryCloud`)

- Läuft **in der vorhandenen Cloud-Task** (`HoyCloud`), als zweiter Schritt in `step()`. So gibt es nie
  zwei TLS-Verbindungen gleichzeitig. Die Task wird dafür auch gestartet, wenn nur die Akku-Cloud
  gewählt ist.
- Gleiche Schutzregel wie heute: nur bei genug freiem internem RAM (70 KB), sonst 3 s später erneut.
- Die Akku-Cloud ruft auch ab, wenn der HIT liefert (Hoymiles-Cloud tut das nicht).
- Anmeldung: HmacSHA1 nach Dyness Open API v1.1 (mbedtls). Ablauf laut der bestehenden
  Home-Assistant-Integration (github.com/shopf/dyness_battery):
  1. `/v1/device/storage/list` (Gerät finden, wenn keine SN eingetragen ist), einmalig `/v1/device/bindSn`
  2. je Intervall **ein** Aufruf, abwechselnd `/v1/device/realTime/data` und `/v1/device/getLastPowerDataBySn`
     (so bleibt es bei einem Aufruf pro Intervall, auch wenn das Limit je Konto gilt)
  3. `/v1/station/info` (Kapazität) beim Start und danach alle 24 h statt eines regulären Aufrufs
  
  Ob das Limit je Endpunkt oder je Konto gilt, ist nicht dokumentiert; mit einem Aufruf pro Intervall ist
  beides abgedeckt. Läuft dieselbe App-ID zusätzlich in der Home-Assistant-Integration, teilen sich beide das
  Limit: Die Akku-Seite weist darauf hin. *(Entscheidung des Entwurfs, nach Prüfung ergänzt.)*
- Schnittstelle als Basisklasse, damit weitere Anbieter dazukommen:
  ```cpp
  struct BatteryCloudDriver { virtual bool poll(const AppConfig&, BatteryInfo&, String &err) = 0; };
  ```
  `BatteryCloud::Dyness` wählt den Dyness-Treiber.
- Fehler (Anmeldung, HTTP, Limit überschritten) stehen in `cloudErr`, auf der Akku-Seite und einmal im
  Ereignisprotokoll.

### Nach Board

| | WT32-ETH01 (kein PSRAM) | T-Connect-Pro, T-POE-Pro, ESP32-P4 |
|---|---|---|
| HIT-Akkuwerte | ja | ja |
| Dyness-Cloud | ja | ja |
| Cloud-Intervall mindestens | 600 s | 300 s |
| Einzelne Zellspannungen | nein (nur Min/Max/Differenz) | 16 Werte |

Unterschieden über `BOARD_HAS_PSRAM` zur Übersetzungszeit.

### Wenn beide Quellen denselben Wert liefern

Die Akku-Seite und `/api/battery` zeigen beide Quellen getrennt. Für Live-Kachel, Home Assistant und KNX gibt
es je Wert **einen** zusammengeführten Wert nach festen Regeln, damit HA-Entitäten stabil bleiben:

| Wert | bevorzugt | sonst |
|---|---|---|
| Zellspannung max/min, Differenz | Cloud (mV genau), wenn jünger als 2 Intervalle | HIT |
| Zell- und Modultemperaturen | HIT (häufiger) | Cloud |
| Lade-/Entladegrenzen | HIT | Cloud |
| Zyklen, Restenergie, Alarm, einzelne Zellen | Cloud | – |

Aus HIT-Werten wird keine Zellspannungsdifferenz gebildet (bei 0,1 V Auflösung sinnlos); ohne Cloud fehlt sie.
HA bekommt pro Wert eine Entität ohne Quelle im Namen und ein Attribut `source` (`hit` oder `cloud`).
*(Entscheidung des Entwurfs, nach Prüfung ergänzt.)*

### Anzeige

- **Neuer Menüpunkt „Akku“** (`/battery`): Karte „vom HIT“ und Karte „Dyness-Cloud“ mit Alter der Daten,
  darunter die Einstellungen (Quelle, Intervalle, App-ID/Secret, Testknopf „Verbindung prüfen“).
  Packinfo als kleine Tabelle. Zellspannungen als Balkenreihe (Boards mit PSRAM).
- **Live-Seite:** in der Akku-Kachel eine Zeile mit Zelltemperatur max und Zellspannungsdifferenz, wenn vorhanden.
- **API:** `GET /api/battery` liefert den Datensatz als JSON (Werte, die fehlen, weglassen).
- Display (T-Connect-Pro): keine Änderung in diesem Schritt.

### Home Assistant (MqttHa)

- Eigener Zustand `<base>/battery`, alle neuen Werte, nur vorhandene Schlüssel (wie bei den Zählerständen).
- Discovery für die vorhandenen Werte, eigenes HA-Gerät „<Gerätename> Akku“ mit `via_device` auf den
  Proxy. Zellspannungen einzeln standardmäßig deaktiviert (`en: false`).
- Veröffentlicht, wenn neue Daten da sind, nicht öfter als `mqttPublishMs`.

### KNX (KnxPoints, KnxIp)

Neue Kanäle **hinten angehängt** (die gespeicherte Zuordnung hängt an der Position):

| id | Wert | DPT |
|---|---|---|
| `bat_cell_tmax` | Zelltemperatur max (zusammengeführt, siehe oben) | 9.001 |
| `bat_cell_tmin` | Zelltemperatur min | 9.001 |
| `bat_cell_vmax` | Zellspannung max | 9.020 (mV) |
| `bat_cell_vmin` | Zellspannung min | 9.020 |
| `bat_cell_vdiff` | Zellspannungsdifferenz | 9.020 |
| `bat_cycles` | Ladezyklen (Cloud) | 12.001 |
| `bat_remain` | Restenergie (Cloud) | 13.013 (kWh) |
| `bat_alarm` | Alarm aktiv (Cloud) | 1.005 |

Fehlt ein Wert, wird der Kanal übersprungen. Die DPT 12.001 und 1.005 braucht der Kodierer neu.
Die Kanalgruppe „Akku“ erscheint in der KNX-Tabelle und im ETS-Export.

## Einschränkungen

- Ob der HIT 1916–1927 für Dyness füllt, ist unbekannt; erst der Test am Gerät zeigt es. Die Seite zeigt
  dann „vom HIT nicht geliefert“.
- Zellspannungen kommen vom HIT nur auf 0,1 V genau (Faktor 10 laut V2.8).
- Die Dyness-API begrenzt die Abrufe (5 min bei 1–2 Modulen, länger bei mehr). Das Intervall wird nicht
  unter die Board-Grenze gesetzt; bei „Limit überschritten“ wird das Intervall bis zum Neustart verdoppelt.

## Dem Builder überlassen

- Genaue Feldnamen der Dyness-Antworten und Signaturdetails (aus der API-Doku bzw. der HA-Integration).
- Aufteilung der Dateien, Namen der Funktionen, Gestaltung der Akku-Seite im vorhandenen Stil.
- Wie die Demo-Werte aussehen.
- Tests: Werkzeug in `tools/` analog zu `sunspec_check.py`, das `/api/battery` abfragt und prüft.
