# Akkudaten Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Zusätzliche Akkuwerte vom HIT (herstellerunabhängig) und optional aus der Dyness-Cloud lesen und in Weboberfläche, Home Assistant und KNX zeigen, ohne HIT-Poll und SunSpec zu beeinträchtigen.

**Architecture:** Ein eigener Datensatz `BatteryInfo` hinter einem Mutex (`BatteryData`). Die HIT-Task liest einen zusätzlichen Registerblock in eigenem Takt; die vorhandene Cloud-Task (`HoyCloud`) ruft nach ihrem eigenen Schritt den Akku-Cloud-Treiber auf. Reine Logik (Registerdekodierung, Zusammenführen, Dyness-Signatur-String, Punkt-Auswertung) liegt in `BatteryLogic.h/.cpp` ohne Netzwerkzugriffe.

**Tech Stack:** Arduino-ESP32 3.x (pioarduino 53.3.13), ArduinoJson 7, mbedtls (HMAC-SHA1, MD5, Base64), PubSubClient, ESPAsyncWebServer.

**Spec:** `docs/superpowers/specs/2026-11-18-akkudaten-design.md`

## Global Constraints

- NVS-Schlüssel höchstens 15 Zeichen: `batena`, `bathit`, `bathitp`, `batcld`, `batchost`, `batcid`, `batcsec`, `batcsn`, `batcp`.
- `batHitPollS` 30–3600, Standard 60. `batCloudPollS` Untergrenze 300 (mit PSRAM) bzw. 600 (ohne `BOARD_HAS_PSRAM`), Standard gleich der Untergrenze.
- `batCloudHost` Standard `open-api.dyness.com`; Basis-URL `https://<host>/openapi/ems-device`, signiert wird nur der Pfad ab `/v1/...`.
- Dyness-Signatur: `Base64(HMAC-SHA1(secret, "POST\n" + ContentMD5 + "\napplication/json\n" + Date + "\n" + path))`, `ContentMD5 = Base64(MD5(body))`, `Date` im RFC-1123-GMT-Format, Header `Authorization: API <appId>:<sig>`, `Content-Type: application/json;charset=UTF-8`. Body kompakt mit sortierten Schlüsseln.
- TLS-Wurzel: DigiCert Global Root G2 (schon in `HoyCloud.cpp`, Kette `*.dyness.com` ← RapidSSL TLS RSA CA G1 ← G2).
- Eine Dyness-Anfrage pro Intervall; Cloud-Abruf nur bei mindestens 70 000 Byte freiem internem RAM.
- Akkudaten nie in `gLive`, nie in SunSpec, nie in History.
- KNX-Kanäle nur hinten anhängen.
- Ohne PSRAM keine Einzelzellen (nur Min/Max/Differenz).
- Texte der Oberfläche auf Deutsch im Stil der vorhandenen Karten; kein Gedankenstrich-Stil.

## Review Focus

1. **HIT liefert den Block nicht (alle 0) oder antwortet mit Exception:** Haupt-Poll und `hitOk` bleiben unberührt, Akku-Karte zeigt „vom HIT nicht geliefert“. Test: Task 2 Schritt „Block leer“.
2. **Dyness antwortet mit 401/Signaturfehler oder `code` ≠ 0/200:** Fehlertext in `cloudErr`, einmal im Ereignisprotokoll, kein Neustart der Anmeldung im Sekundentakt. Test: Task 3 Schritt „falsches Secret“.
3. **Rate-Limit (`429` / `TOO_MANY_REQUESTS`):** Intervall verdoppelt sich bis 3600 s, Hinweis in der Karte. Test: Task 3.
4. **Secret leer im Formular:** bleibt unverändert, wird nie ausgeliefert. Test: Task 1 (`/api/config` enthält nur `batCloudSecretSet`).
5. **Uhrzeit noch nicht per NTP gesetzt:** keine Cloud-Anfrage (Date-Header wäre falsch → 401), Karte zeigt „wartet auf Uhrzeit“. Test: Task 3.

Prüfweg: Ein Host-Compiler ist auf diesem Rechner nicht vorhanden. Geprüft wird jede Aufgabe mit dem Bau aller vier Umgebungen (`pio run -e t-connect-pro -e wt32-eth01 -e t-poe-pro -e p4-poe-eth`), `node --check` des eingebetteten JavaScripts und dem neuen Werkzeug `tools/battery_check.py` gegen ein Gerät im Demo-Modus. Die Signatur prüft `tools/battery_check.py --sign-selftest` gegen das Python-Referenzverfahren aus der Dyness-Integration (bekannter Testvektor, siehe Task 3).

---

### Task 1: Einstellungsblock „Akku“

**Files:**
- Modify: `include/AppConfig.h` (Enum + Felder), `src/ConfigStore.cpp` (laden/speichern), `src/WebConfig.cpp` (`applyJson`, `GET /api/config`, `features` in `/api/status`, Funktionskarte, `FEAT_KEY`, `formFeat`, `navFeat` in allen Seiten, `FEATS` im Assistenten, neue Karte `#akku`, Menüeintrag), `tools/patch_sidebars.py` (Menüeintrag).

**Interfaces:**
- Produces: `enum class BatteryCloudType : uint8_t { None = 0, Dyness = 1 }; constexpr uint8_t BATTERY_CLOUD_COUNT = 2;` Felder `bool batEnabled=true, batHitEnabled=true; uint16_t batHitPollS=60; BatteryCloudType batCloud=None; char batCloudHost[64]="open-api.dyness.com", batCloudAppId[64]="", batCloudSecret[80]="", batCloudSn[40]=""; uint16_t batCloudPollS=BAT_CLOUD_MIN_S;` und `constexpr uint16_t BAT_CLOUD_MIN_S` (300 / 600 je nach `BOARD_HAS_PSRAM`).
- Feature-Schlüssel `akku` in `/api/status` → `features.akku = batEnabled`.

- [ ] Schritt 1: Felder und Enum in `AppConfig.h` anlegen (Kommentar wie beim `InverterType`).
- [ ] Schritt 2: Laden/Speichern in `ConfigStore.cpp` mit den Schlüsseln aus den Global Constraints; beim Laden `batCloud` auf `< BATTERY_CLOUD_COUNT` begrenzen, Intervalle in ihre Bereiche klemmen.
- [ ] Schritt 3: `applyJson`: Bools, `batCloud` mit Bereichsprüfung, Intervalle klemmen, Strings mit `strlcpy`, Secret nur bei nicht leerem Wert. `GET /api/config`: alle Felder außer Secret, dazu `batCloudSecretSet` und `batCloudMinS`.
- [ ] Schritt 4: Funktionskarte: Zeile `<label><input type="checkbox" data-feat="batEnabled"/> <span><b>Akkudaten</b>: Temperaturen, Zellspannungen und Grenzwerte vom HIT, optional aus der Akku-Cloud</span></label>`; `FEAT_KEY.batEnabled='akku'`, `formFeat` um `akku:g('batEnabled')`, `navFeat` um `akku:F.akku===false` in allen fünf Seiten; Assistent `FEATS` um `['akku','batEnabled','Akkudaten','Akkudaten']`.
- [ ] Schritt 5: Menüeintrag „Akku“ unter „Datenquellen“ nach „Hoymiles-Cloud“, `href="/config#akku"`, Symbol Batterie (`<rect x="3" y="7" width="16" height="10" rx="2.5"/><path d="M21 10.5v3M7 12h8"/>`), in allen Seiten und in `patch_sidebars.py`.
- [ ] Schritt 6: Karte `<div class="card" id="akku" data-sub="Zusätzliche Akkuwerte vom HIT und aus der Cloud des Akku-Herstellers.">` mit: `batEnabled` (versteckt gespiegelt über die Funktionskarte, sichtbar als „aktiv“), `batHitEnabled`, `batHitPollS` (number, min 30 max 3600), `batCloud` (select: 0 „keine“, 1 „Dyness“), `batCloudHost`, `batCloudAppId`, `batCloudSecret` (password, Platzhalter „leer lassen = unverändert“), `batCloudSn` (Platzhalter „leer = automatisch“), `batCloudPollS`; Hinweis: App-ID und Secret im Portal ems.dyness.com unter Entwicklerzentrum, API-Verwaltung; teilt sich das Limit mit anderen Programmen, die dieselbe App-ID nutzen. Platz `<div id="batBox">` (column-span:all) für Task 4.
- [ ] Schritt 7: Bauen (alle vier Umgebungen), `node --check` der Skripte. Erwartet: SUCCESS, keine Syntaxfehler.
- [ ] Schritt 8: Commit `Akku: Einstellungen und Funktionsauswahl.`

### Task 2: Datensatz und HIT-Abfrage

**Files:**
- Create: `include/BatteryData.h`, `src/BatteryData.cpp`, `include/BatteryLogic.h`, `src/BatteryLogic.cpp`
- Modify: `src/HitClient.cpp`, `src/WebConfig.cpp` (`GET /api/battery`), `src/main.cpp` (`BatteryData::begin()` vor `HitClient::begin`)

**Interfaces:**
- Produces `struct BatteryInfo` wie in der Spec (Felder unten), `namespace BatteryData { void begin(); BatteryInfo snapshot(); void updateHit(const BatteryHit&); void updatePacks(const BatteryPack*, uint8_t n); void updateCloud(const BatteryCloudData&); void setCloudError(const char*); void toJson(JsonDocument&, const AppConfig&); }`
- `struct BatteryHit { bool valid; uint32_t atMs; float maxChargeA, maxDischargeA, chargeCutV, dischargeCutV, modTempMax, modTempMin, cellTempMax, cellTempMin, modVMax, modVMin, cellVMax, cellVMin; }` (fehlend = NAN)
- `struct BatteryPack { char sn[33]; uint32_t ratedCap; uint32_t bmsFw; uint16_t dcdcSw, dcdcHw; }`
- `bool decodeHitBattery(const uint16_t regs[12], BatteryHit &out)` in `BatteryLogic` (false = Block leer).
- `void decodeHitPack(const uint16_t regs[28], BatteryPack &out)`.

- [ ] Schritt 1: `decodeHitBattery` nach der Tabelle der Spec: U16 ÷10 für Ströme/Spannungen, I16 ×1 für Temperaturen; alle zwölf 0 → `false`; einzeln NAN bei Spannung 0, U16 0xFFFF, I16 0x8000. `decodeHitPack`: SN aus 16 Registern High-Byte zuerst, nach dem ersten 0-Byte abschneiden, nicht druckbare Zeichen durch `?`; BMS-SW U32 aus +22/+23, Kapazität U32 aus +26/+27.
- [ ] Schritt 2: `BatteryData` mit `SemaphoreHandle_t`, `snapshot()` kopiert unter dem Mutex. `toJson` schreibt `hit:{...}`, `packs:[...]`, `cloud:{...}`, `merged:{...}` (Task 4 füllt `merged`), Alter in Sekunden, nur vorhandene Werte (`isnan` → weglassen).
- [ ] Schritt 3: In `HitClient::doPoll` nach dem erfolgreichen Haupt-Poll: wenn `local.batEnabled && local.batHitEnabled` und `batNext` erreicht: `rd(1916, 12, regs)`; Ergebnis über `decodeHitBattery` an `BatteryData::updateHit`. Fehler: einmal `EventLog::add('W', "Akku", "HIT liefert keine Akkudetails: %s", HitBus::lastError())`, danach erst nach Erfolg wieder. Packinfo beim ersten Mal und stündlich: `HitBus::readHolding(8000, 5, …)`, Anzahl = Reg 8002 (begrenzt 0..4), je Pack `readHolding(8100 + 50*i, 28, …)`; 40 ms Pause zwischen Anfragen wie im Haupt-Poll.
- [ ] Schritt 4: `doDemo` füllt plausible Werte (Zelltemp 18–24 °C, Zellspannung 3,30–3,34 V, 1 Pack `DEMO-PACK-0001`).
- [ ] Schritt 5: `GET /api/battery` (mit `requireAuth`) liefert `BatteryData::toJson`.
- [ ] Schritt 6: Test „Block leer“: Werkzeug `tools/battery_check.py` (Task 7) prüft im Demo-Modus `hit.cellTempMax` vorhanden; Codeprüfung: bei `decodeHitBattery`==false wird `updateHit` mit `valid=false` aufgerufen und `gLive` nicht berührt.
- [ ] Schritt 7: Bauen alle vier Umgebungen. Erwartet SUCCESS.
- [ ] Schritt 8: Commit `Akku: zusätzliche Batteriewerte vom HIT.`

### Task 3: Akku-Cloud-Treiber Dyness

**Files:**
- Create: `include/BatteryCloud.h`, `src/BatteryCloud.cpp`, `src/BatteryDyness.cpp`
- Modify: `src/HoyCloud.cpp` (CA-Kette nach außen, Aufruf in `taskMain`, Reset bei neuer Konfiguration), `include/HoyCloud.h` (`extern const char CLOUD_CA_DIGICERT[];`)

**Interfaces:**
- `struct BatteryCloudData { bool valid; uint32_t atMs; char sn[40]; char model[24]; float soc, soh, powerW, voltV, currA, tempMax, tempMin, mosTemp, cellVMax, cellVMin, cellDiffMv, usableKwh, remainKwh, capKwh, chargeLimA, dischargeLimA; float cycles; bool alarm; uint8_t cells; float cellV[16]; }` (in `BatteryData.h`)
- `struct BatteryCloudDriver { virtual ~BatteryCloudDriver() = default; virtual bool poll(const AppConfig &cfg, BatteryCloudData &io, String &err) = 0; virtual void reset() = 0; };`
- `namespace BatteryCloud { void step(const AppConfig &cfg); void reset(); }` – aufgerufen aus der Cloud-Task.
- In `BatteryLogic`: `String dynessStringToSign(const char *method, const char *contentMd5, const char *date, const char *path)`; `void dynessParsePoints(JsonArrayConst points, BatteryCloudData &io)`.

- [ ] Schritt 1: `CLOUD_CA_DIGICERT` aus dem anonymen Namespace von `HoyCloud.cpp` lösen (gleicher Inhalt), `HOY_CA` verweist darauf.
- [ ] Schritt 2: `BatteryCloud::step`: nichts tun ohne `batEnabled` oder bei `batCloud==None`; Netz-Wartezeit 5 s wie `HoyCloud::step`; ohne gültige Uhrzeit (`time(nullptr) < 1700000000`) Fehler „wartet auf Uhrzeit“; RAM-Schwelle 70 000; Intervall `max(batCloudPollS, BAT_CLOUD_MIN_S) * backoff`. Bei Fehler `BatteryData::setCloudError`, Ereignisprotokoll nur beim Wechsel ok → Fehler.
- [ ] Schritt 3: Dyness-Treiber, Ablauf je Aufruf genau eine Anfrage:
  - ohne SN: `/v1/device/storage/list` `{}` → `data.list[]`, bevorzugt `deviceSn` mit Endung `-BMS`/`-BDU`, sonst erstes; `collectorSn` und `deviceModelName` merken; danach einmal `/v1/device/bindSn` `{deviceSn, collectorSn?}` (Antwortcode 0/200/500 gilt als ok).
  - danach im Wechsel: `/v1/device/realTime/data` `{collectorSn?, deviceSn}` → `data[]` mit `pointId`/`pointValue` → `dynessParsePoints`; `/v1/device/getLastPowerDataBySn` `{collectorSn?, deviceSn, pageNo:1, pageSize:1}` → letzter Eintrag mit `soc`: `soc`, `realTimePower` → `powerW`, `realTimeCurrent` → `currA`.
  - beim Start und alle 24 h statt eines regulären Aufrufs: `/v1/station/info` `{deviceSn}` → `data.batteryCapacity` → `capKwh`.
  - Erfolg: `code` "0"/"200" bzw. 0. Rate-Limit: `code` "429", HTTP 429 oder `sourceCode` `TOO_MANY_REQUESTS` → backoff ×2 bis 3600 s.
- [ ] Schritt 4: `dynessParsePoints` (PowerBrick laut Integration): Variante A, wenn Punkt `600` vorhanden: 600 V, 700 A, 800 SoC, 1200 SoH, 1300/1500 Zelle max/min (V), 1800/2000 Temp max/min, 2300 MOSFET, 900 Zyklen (≠0), 3800/3900 Lade-/Entladestrom-Grenze (>0), 4100 Alarm (≠"0"). Variante B sonst: 13500 V, 13400 A, 13900 Zyklen, Temp max/min aus 12500/12600/12700/12800 (>0), 18600/19200 Strom-Grenzen. Einzelzellen beide Varianten: Punkt `10200 + i*100`, i=1..16, Wert > 0 (nur mit PSRAM speichern); Max/Min/Differenz (mV) daraus, falls nicht direkt geliefert. Restenergie = `capKwh * soh/100 * soc/100`, wenn alles bekannt.
- [ ] Schritt 5: HTTPS-POST mit `NetworkClientSecure` (CA `CLOUD_CA_DIGICERT`, Handshake 10 s), Header nach Global Constraints; MD5/HMAC/Base64 mit mbedtls; `Date` aus `gmtime_r` + `strftime("%a, %d %b %Y %H:%M:%S GMT")`; Body mit ArduinoJson aus einem Objekt, dessen Schlüssel schon alphabetisch eingefügt werden.
- [ ] Schritt 6: `HoyCloud::taskMain` ruft nach `step()` `BatteryCloud::step(local)`; `takeConfig` ruft bei Änderung `BatteryCloud::reset()`.
- [ ] Schritt 7: Signatur-Selbsttest: `tools/battery_check.py --sign-selftest` berechnet für Secret `test-secret`, Body `{"deviceSn":"X"}`, Date `Tue, 18 Nov 2026 10:00:00 GMT`, Pfad `/v1/device/realTime/data` die Signatur nach dem Python-Referenzverfahren und gibt sie aus; derselbe Vektor steht als Kommentar mit erwartetem Ergebnis in `BatteryDyness.cpp`, und `BatteryCloud` schreibt beim ersten Start im Demo-Modus die eigene Berechnung ins serielle Log (`Dyness sign selftest: ok/FAIL`).
- [ ] Schritt 8: Bauen alle vier Umgebungen. Erwartet SUCCESS.
- [ ] Schritt 9: Commit `Akku: Dyness-Cloud als Akku-Cloud-Treiber.`

### Task 4: Zusammenführen und Anzeige

**Files:**
- Modify: `src/BatteryLogic.cpp` (`mergeBattery`), `src/BatteryData.cpp` (`merged` in JSON), `src/WebConfig.cpp` (Karte `#akku` Datenteil, Live-Seite)

**Interfaces:**
- `struct BatteryMerged { float cellTempMax, cellTempMin, modTempMax, modTempMin, cellVMax, cellVMin, cellDiffMv, chargeLimA, dischargeLimA, cycles, remainKwh; int8_t alarm; /* -1 unbekannt */ const char *srcCellV, *srcTemp, *srcLim; }`
- `BatteryMerged mergeBattery(const BatteryInfo &b, uint32_t nowMs, uint32_t cloudIntervalMs)`

- [ ] Schritt 1: Regeln aus der Spec-Tabelle: Zellspannungen aus Cloud, wenn `cloud.valid` und jünger als `2*cloudIntervalMs`, sonst HIT (ohne Differenz); Temperaturen und Grenzen HIT, sonst Cloud; Zyklen, Restenergie, Alarm nur Cloud. `src*` = `"hit"`/`"cloud"`/`nullptr`.
- [ ] Schritt 2: `toJson` schreibt `merged` inkl. `source`-Feldern.
- [ ] Schritt 3: Karte `#akku`, `#batBox`: zwei Tabellen „vom HIT“ und „Akku-Cloud“ (Alter, Werte mit Einheit, „nicht geliefert“ bei fehlendem Block, Fehlertext), Packtabelle (SN, BMS-Firmware, Kapazität roh), Zellbalken (16 schmale Balken, Höhe relativ zu Min/Max, Beschriftung mV) nur wenn `cloud.cells` vorhanden; Aktualisierung alle 10 s solange die Karte sichtbar ist.
- [ ] Schritt 4: Live-Seite: in der Akku-Kachel eine Zeile `Zelle max <T> °C · Spreizung <mV> mV`, nur wenn `features.akku` und Werte vorhanden (eigener Abruf `/api/battery` alle 30 s).
- [ ] Schritt 5: Bauen, `node --check`. Erwartet SUCCESS.
- [ ] Schritt 6: Commit `Akku: Werte zusammenführen und anzeigen.`

### Task 5: Home Assistant

**Files:** Modify `src/MqttHa.cpp`, `include/MqttHa.h`

**Interfaces:** `void MqttHa::publishBattery();` aufgerufen aus `MqttHa::loop()` höchstens alle `mqttPublishMs`, nur bei `batEnabled` und geändertem `atMs` von HIT oder Cloud.

- [ ] Schritt 1: Zustandstopic `<base>/battery`, JSON aus `mergeBattery` plus Cloud-SoC/SoH, nur vorhandene Werte, `source_cellv`, `source_temp`.
- [ ] Schritt 2: Discovery je Wert, `uniq_id` `<base>_akku_<key>`, Gerät `ids:["<base>_akku"]`, `name:"<deviceName> Akku"`, `via_device:<base>`; Klassen: Temperatur `temperature`/°C/`measurement`, Spannung `voltage`/V, Differenz mV `voltage`, Strom `current`/A, Restenergie `energy_storage`/kWh/`measurement`, Zyklen ohne Klasse `total_increasing`, Alarm als `binary_sensor` (`homeassistant/binary_sensor/...`, `payload_on:1`). Einzelzellen `en:false`. Discovery einmal nach Verbindung, wenn `batEnabled`.
- [ ] Schritt 3: Bauen. Commit `Akku: Werte an Home Assistant.`

### Task 6: KNX

**Files:** Modify `include/KnxPoints.h`, `src/KnxIp.cpp`, `src/WebConfig.cpp` (`kinds[]`, `KNX_DPT`, `knxGrp`)

**Interfaces:** neue `KnxValKind` hinten: `Temp, MilliVolt, Count, Flag`; neue `KnxChId` hinter `KNX_LPP`: `KNX_AKKU_TMAX, KNX_AKKU_TMIN, KNX_AKKU_VMAX, KNX_AKKU_VMIN, KNX_AKKU_VDIFF, KNX_AKKU_CYCLES, KNX_AKKU_REMAIN, KNX_AKKU_ALARM`; ids `akku_tmax, akku_tmin, akku_vmax, akku_vmin, akku_vdiff, akku_cycles, akku_remain, akku_alarm`; Standard-DPT 9001, 9001, 9020, 9020, 9020, 12001, 13013, 1005.

- [ ] Schritt 1: Katalog erweitern (Labels: „Zelltemperatur max“, „Zelltemperatur min“, „Zellspannung max“, „Zellspannung min“, „Zellspannung Spreizung“, „Ladezyklen“, „Restenergie“, „Akku-Alarm“). Spannungen in mV an KNX.
- [ ] Schritt 2: `readPoint` liefert für die neuen Kanäle die Werte aus `mergeBattery(BatteryData::snapshot(), …)` (einmal pro `publish()` berechnen); fehlender Wert (NAN bzw. alarm −1) → Kanal überspringen. Ohne `batEnabled` überspringen.
- [ ] Schritt 3: Kodierer: 12001 → U32 BE; 13013 → I32 BE (kWh gerundet); 1005 → 1 Bit im APCI-Byte (`sendGroupWrite` mit Datenlänge 0 und Wert im unteren Bit von 0x80); 9020 über den vorhandenen DPT-9-Zweig. `significantChange`: Temp 0,5, MilliVolt 5, Count 1, Flag jede Änderung.
- [ ] Schritt 4: UI: `kinds[]` um `"Temp","MilliVolt","Count","Flag"`; `KNX_DPT` um `Temp:[[9001,'9.001 °C'],[14068,'14.068 °C']]`, `MilliVolt:[[9020,'9.020 mV']]`, `Count:[[12001,'12.001']]`, `Flag:[[1005,'1.005 Alarm']]`, `EnergyKwh` um `[13013,'13.013 kWh']`; `knxGrp`: `/^akku/` → `'Akku-Details'` (vor der `bat`-Regel). 14068 über den vorhandenen F32-Zweig.
- [ ] Schritt 5: Bauen, `node --check`. Prüfen, dass `ConfigStore` alte, kürzere `knxch`-Listen lädt (unverändert, Ladeschleife nimmt das Minimum). Commit `Akku: KNX-Kanäle.`

### Task 7: Prüfwerkzeug, Doku, Abschluss

**Files:** Create `tools/battery_check.py`; Modify `README.md` (Abschnitt „Seit v0.5.8“ und Funktionsliste), `docs/superpowers/specs/...` nicht ändern.

- [ ] Schritt 1: `battery_check.py <host> [--user admin --pass CODE]`: holt `/api/battery`, prüft Struktur (`hit`, `packs`, `cloud`, `merged`), druckt eine Tabelle; mit `--sign-selftest` nur den Signaturvektor aus Task 3 berechnen und ausgeben.
- [ ] Schritt 2: README: Akkudaten in „Seit v0.5.8“, kurze Anleitung Dyness-App-ID.
- [ ] Schritt 3: Alle vier Umgebungen bauen, `node --check`, `python tools/battery_check.py --sign-selftest`. Erwartet: SUCCESS, Signatur gleich dem Kommentar in `BatteryDyness.cpp`.
- [ ] Schritt 4: Commit `Akku: Prüfwerkzeug und Doku.`
