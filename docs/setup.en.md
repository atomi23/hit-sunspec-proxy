# Getting started and tools

[Deutsch](./setup.md) · **English** · [Back to overview](../README.en.md)

## Getting started

1. First flash via USB: `pio run -e t-connect-pro -t upload`\
   (or the release `firmware.bin` under *Firmware and reset* via OTA — if OTA hangs, restart first)
2. Connect to the device Wi-Fi `HIT-Proxy-XXXX`. With display: Wi-Fi name, access code and QR code are shown
   on the display. Without display: password `hitproxy1`, login with `admin` and `hitproxy1`.
3. Setup opens by itself on a phone, otherwise open `http://192.168.4.1/` in a browser. The wizard
   asks what is needed; without a display, setting your own password is mandatory.
4. Consolinno: device as Fronius Hybrid / SunSpec, **proxy IP**, port **1502**, unit ID **1**\
   (a scan on 502 only does not find the proxy while SunSpec listens on 1502)
5. When HTTPS is active, the UI runs at `https://<esp-ip>/`; otherwise it uses HTTP (for example on the
   WT32-ETH01). To open it over HTTPS without a warning, download the device CA
   under *Network and access* and install it once as a trusted root CA
   (see [HTTPS](web-ui.en.md#https)). Alternatively upload your own certificate.

If there is already another SunSpec source, you can compare both:
`python tools/sunspec_check.py <old-source> --compare <esp-ip>`. Other programs that read the HIT via Modbus
then use `<esp-ip>:502` (enable the Modbus gateway).

Pins: [`PINS.md`](../PINS.md)

## Tools

| Script | Purpose |
|--------|---------|
| `tools/sunspec_check.py` | Read the SunSpec chain, compare two devices (register offset) |
| `tools/parity_check.py` | Check an existing SunSpec map against the ESP's conversion of the same HIT data (read-only) |
| `tools/make_vlw.py` | Generate display fonts (VLW) from Barlow |
| `tools/embed_fonts.py` | Embed web fonts (WOFF) |
| `tools/https_check.py` | Check HTTPS: redirect, saving, certificate upload/rejection, optionally OTA |
| `tools/esp_soak.py` | Load test of the web UI over HTTP or HTTPS, also with a connection held open |
| `tools/modbus_watch.py` | Read the SunSpec identifier continuously, report slow responses and outages |
| `tools/serial_log.py` | Record the serial log |
| `tools/battery_check.py` | Check battery data via `/api/battery` and show it as a table; `--sign-selftest` checks the Dyness signature |

Credentials for the tools come from `ESP_USER` / `ESP_PASS`, never from the repo.
