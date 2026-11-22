# Integrations

[Deutsch](./integrations.md) · **English** · [Back to overview](../README.en.md)

## Architecture

```
                         ┌── SunSpec :1502 (optional) ──→ Consolinno HEMS
Hoymiles HIT ──TCP/RS485─┤── Modbus gateway :502 ──→ Home Assistant, other Modbus clients
   (one connection)      ├── MQTT + Discovery ──→ Home Assistant
          ESP32          ├── KNX IP Routing ──→ KNX bus
                         └── Display, web UI, optional Tasmota meter
```

| Path | Required? | Role |
|------|-----------|------|
| HIT (Modbus TCP or RS485) | for direct HIT access | Primary data source, the only connection to the HIT; MQTT and Hoymiles cloud can supply fallback live data |
| SunSpec :1502 | for Consolinno only | Default port for GEN24 emulation (models 1/103/120–124/160; optional meter 203); the service can be disabled |
| Modbus gateway | no | Other Modbus clients read the HIT through the ESP |
| MQTT | no | HA sensors via discovery, optionally MQTT as fallback source |
| KNX IP | no | Multicast routing of selectable live values; on the T-Connect-Pro it also receives commands for the display and LED strip |
| Tasmota | no | Grid meter for the live view and optionally SunSpec model 203 |
| Hoymiles cloud | no | S-Miles read-only, fallback source when HIT and MQTT fail |
| HIT write | no | Battery limits 306–309, APC 279/280, off by default |

<a id="bluetooth"></a>

## Bluetooth LED strip via the web UI and KNX (T-Connect-Pro)

LED strips driven by the **Lotus Lantern** app (Bluetooth names `ELK-BLEDOM…`, `MELK-…`, `LEDBLE…`) can be switched
by KNX. Under **Settings → KNX**, search for the strip, select it, save, and check it with **Test: on/off**. Then
assign group addresses to the receive objects: **switch** (DPT 1.001), **brightness** (DPT 5.001) and **RGB color**
(DPT 232.600). The ESP connects only to send and releases the strip after 10 seconds, because the strip accepts a
single Bluetooth connection; the app on the phone must be closed. Available on the T-Connect-Pro (ESP32-S3) only: the
ESP32 boards lack the memory for Bluetooth and the ESP32-P4 has no Bluetooth. The command set comes from public
protocol descriptions (ELK-BLEDOM).

Once configured, the strip can also be switched, dimmed and set to a color on the **Bluetooth** page
without assigning any KNX group addresses. The page shows the connection state, the last error,
the last values sent, and LED entries from the event log. The values sent are not feedback from the strip.
