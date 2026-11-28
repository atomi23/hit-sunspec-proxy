# Release history

[Deutsch](./changelog.md) · **English** · [Back to overview](../README.en.md)

## New in v0.7.2
- **Web UI stays reachable:** If the web server or the HTTPS proxy hung, SunSpec kept running but the pages would not open until the device was power-cycled. A self-test now queries port 80 via 127.0.0.1 every 30 s and restarts the device after about 2 minutes without an answer. If only port 443 hangs, port 80 stops redirecting to HTTPS at once; after 10 minutes the device restarts as well. Every restart by the self-test is logged with its reason in the system log.

<details>
<summary><b>Older versions</b> (v0.7.1 to v0.5.6)</summary>

### v0.7.1
- **Bluetooth LED strip via KNX (T-Connect-Pro only):** The smallest brightness value (DPT 5.001, raw value 1) dims the strip to 1 % instead of switching it off. Only 0 switches off.
- **"Bluetooth" page:** "Last sent" shows every command the strip accepted, even if a later command of the same sequence fails. Rapid use no longer starts parallel requests (3 clicks: 10 instead of 51 requests to the device).
- **SunSpec and Modbus gateway:** When all connection slots are taken, the longest-idle connection is dropped instead of always the first one. Until now that could hit the active HEMS.
- **Tasmota meter:** A slow or silent meter no longer stalls SunSpec and the Modbus gateway (previously up to 3 s per poll).
- **Web UI:** Is gzip-compressed at build time; the firmware is about 105 KiB smaller per board (WT32-ETH01: 88.8 % used instead of 94.3 %).

### v0.7.0
- **Bluetooth settings on the "Bluetooth" page (T-Connect-Pro only):** Strip search, name, address and power-on command now sit where the strip is controlled, instead of under *Settings → KNX*. Only the group addresses stay there.
- **"Overview" page:** Shows hardware (board, chip, memory, flash), software (firmware, build date, Arduino/ESP-IDF, uptime, last start), network and services, and the load: CPU load per core, memory, PSRAM and file system.
- **HTTPS despite Bluetooth:** Since v0.5.23 the T-Connect-Pro was about 40 KB of internal RAM short for HTTPS ("Not enough RAM for HTTPS; HTTP only"). On boards with PSRAM, TLS now lives in PSRAM.

### v0.6.0
- **"Bluetooth" page (T-Connect-Pro only):** Shows the connection state of the LED strip and the last error, switches the strip on and off and sets brightness and color. It shows what the gateway sent last; the strip reports nothing back.
- **Bluetooth log:** Every command sent to the strip is in the event log with its bytes, as is every connection. Error messages now name the reason (not reachable or no matching write channel).

### v0.5.23
- **Bluetooth LED strip via KNX (T-Connect-Pro only):** LED strips driven by the *Lotus Lantern* app can be switched (DPT 1.001), dimmed (DPT 5.001) and colored (DPT 232.600) through three KNX objects. Set up with strip search and test under *Settings → KNX*. The commands come from public protocol descriptions and have not yet been tried on a real strip.
- **Web UI:** Decimal numbers follow the selected language (comma or point).
- **HIT inverter:** A truncated RS485 response is reported as an error.
- **Data origin:** Is reset when all data sources have expired.

### v0.5.22
- **HTTPS on Windows:** CA installation steps now select the root certificate store manually, so Chrome trusts the device CA.
- **Log out:** Button on the main pages of the web UI, also over HTTPS. If the browser keeps the login, the hint to close the browser helps.

### v0.5.20
- **Web UI:** Loads the layout with the page instead of requesting it separately after a restart.

### v0.5.19
- **German and English:** Added missing texts in backup and restore, HTTPS, downloads and status messages. New log entries follow the selected language.

### v0.5.18
- **CA download:** Now delivers the certificate file instead of the certificate information as JSON.

### v0.5.17
- **HTTPS without browser warning:** The device CA signs the server certificate; install its CA certificate once as a trusted root CA.
- **Download the CA certificate:** Public and without login at `/api/tls/ca.crt`, also reachable over HTTP.
- **Web UI and OTA stay reachable:** If switching to the CA fails, a matching stored certificate or a self-signed fallback certificate is used. The next boot tries the switch again.

### v0.5.16
- **Logs and history:** Download the event and debug logs as text files and the history data as CSV.
- **Display on the T-Connect-Pro:** Weekly schedule with separate on and off times per weekday. An optional KNX object (DPT 1.001) switches the backlight on or off.
- **Firmware and reset:** Back up and restore settings as JSON. Passwords and the access code can be excluded from the export.

### v0.5.15
- **HIT polling:** Starts only once the network and IP address are ready, so network-related failed attempts at boot no longer end up in the statistics. RS485 and demo are unchanged.
- **Poll interval:** Default is now 30 s. Existing settings are kept; if needed, set **Poll (ms)** under **HIT inverter** to `30000`.

### v0.5.14
- The English web UI now also covers all settings submenus, the logs, and firmware and reset.
- The history has English chart labels for screen readers. On small or short screens the chart areas no longer overlap.
- For OTA, pick the file from the release that matches the board. For the **T-Connect-Pro with display** it is `firmware.bin`, not `firmware-t-poe-pro.bin`.

### v0.5.13
- **New setup wizard** with two paths: *Simple* asks questions ("Do you have a Consolinno HEMS?",
  "Do you have KNX in the house?" …) and switches the services on from the answers; *Expert* shows all fields (ports,
  unit IDs, RS485). Credentials for Hoymiles cloud, Dyness cloud, MQTT, Tasmota and home Wi-Fi are entered
  right in the wizard; both cloud logins can be tested there. German and English
- **Boards without display:** Until setup, the default password `hitproxy1` applies (device Wi-Fi and
  login with `admin`). The wizard requires your own password with at least 8 characters; after that the
  default password is invalid everywhere, and the device Wi-Fi also uses your own password
- **Boards with display** as before: random access code and QR code on the display
- Devices that are already set up keep their existing access code when updating

### v0.5.12
- **Hoymiles cloud without plant ID:** If the field is empty, the proxy takes the account's first plant and
  writes its number to the log. Previously nothing happened without an ID, not even a message
- **Dyness cloud on boards without PSRAM:** The power query returns more than 50 KB and failed for lack of memory;
  it is now read entry by entry. Responses that arrive in pieces no longer abort
- Battery page: serial numbers and error text arrived garbled
- Battery cloud: after a connection error, it retries after 30 s instead of waiting for the full interval

### v0.5.11
- **Device Wi-Fi follows the switch:** If *Keep the device's Wi-Fi network enabled (emergency access)* is off and the device is on the cable,
  the Wi-Fi now really stays off (previously it always ran without home Wi-Fi). As emergency access it turns on
  by itself if the device has no address on the cable for one minute, and off again when the cable is back
- **HTTPS only with enough memory:** The check at boot now also accounts for a browser's parallel
  connections. The **WT32-ETH01** (no PSRAM) therefore always runs over HTTP, even with
  Wi-Fi switched off; boards with PSRAM keep using HTTPS

### v0.5.10
- **Boards without PSRAM (WT32-ETH01):** The settings page arrived empty, and HTTP redirected to an HTTPS that
  dropped every connection for lack of memory, so the UI was unreachable. Pages are now sent
  directly from flash. If the RAM is not enough for HTTPS, it does not start and the UI runs
  over HTTP (shown under *Network and access* and in the log)
- Diagnostic output on the serial port only with the build flag `-DTLS_DIAG=1`

### v0.5.9
- New design **Blueprint** with its own controls: menu bar at the top (at the bottom on phones), settings as a
  sliding sheet, and an energy band instead of the node diagram on the live page. The band width matches
  the power; PV covers the house first, then the battery, then the grid feed-in
- New design **Steel**: light with a dark sidebar
- **Setup on the phone:** In the device Wi-Fi the setup page opens by itself (captive portal)
- Grid power on the live page is also readable in light designs
- New build variant **`p4-poe-eth`** for the [Waveshare ESP32-P4-WIFI6-POE-ETH](https://www.waveshare.com/esp32-p4-wifi6-poe-eth.htm):
  Ethernet with PoE, Wi-Fi via the ESP32-C6; not yet tested on hardware
- Printable screwless DIN rail enclosures (printed-on clip, snap-on lid):
  [WT32-ETH01, 2 modules](../hardware/wt32-din/) as a PoE-only variant or with RS485/step-down,
  [ESP32-P4-POE-ETH, 3.5 modules](../hardware/p4-poe-din/)
- **Battery data** under *Data sources, Battery*: cell temperatures, cell voltages, charge and discharge limits and
  pack info from the HIT, plus optionally SoC, SoH, cycles and remaining energy from the **Dyness cloud**. The live page
  shows the merged values, Home Assistant and KNX get them as separate data points.
  Dyness: create an app ID and secret in the portal [ems.dyness.com](https://ems.dyness.com) under *Developer center,
  API management* (the secret is shown only once) and enter them under *Battery*; if the
  Home Assistant integration uses the same app ID, both share the request limit

### v0.5.8
- **Feature selection** in the setup wizard and under *Settings, Features*: Consolinno/SunSpec,
  Modbus gateway, Home Assistant, KNX, Tasmota meter and Hoymiles cloud can be selected individually
- Services that are not selected do not start and disappear from the menu and live page; SunSpec (port 1502) can be switched off
- Inverter type prepared as a setting (so far only Hoymiles HIT)
- OTA update from older versions: settings are kept, no new setup wizard

### v0.5.7
- **KNX:** Mapping table grouped by area, rows shaded, invalid or duplicate addresses in red;
  **import/export of group addresses in ETS format** (XML and CSV)
- New build variant **`wt32-eth01`**; `t-poe-pro` fixed (clock, PHY reset, RS485 pins, 16 MB flash, PSRAM per LilyGO)
- CSRF protection: third-party web pages can no longer trigger anything via a stored login
- An OTA update belongs to exactly one request; login is checked before large requests are accepted
- Home Assistant: no more meter readings of 0 before the first data (this caused double-counted energy)
- Cloud: meter readings in Wh are always converted correctly, also below 10 kWh
- "Write to the HIT now" writes the values just saved; "Test HIT" in the setup waits for a fresh poll
- Tasmota: crash with a numeric meter number fixed, credentials URL-encoded
- History survives a power failure while saving; KNX type numbers for current/frequency corrected (14.019/14.033)
- Invalid inputs (port 0, unit ID 0, empty Wi-Fi name …) are no longer saved

### v0.5.6
- HIT poll in its own FreeRTOS task (UI/Consolinno stay responsive)
- Tasmota selectable as live grid meter; optional SunSpec **Model 203** (3-phase spoof from the total + HIT voltages)
- KNX DPT9 scaled correctly; FC16 power limit only after a complete register set
- Web callbacks (poll, HIT write, Tasmota probe) run in the main loop, not in the async task
- Passwords are no longer returned in plain text in `/api/config`
- Robust against `millis()` rollover; platform `espressif32 @ 53.3.13` pinned; custom 16 MB partition table

</details>
