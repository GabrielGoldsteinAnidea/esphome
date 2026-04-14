# Air Alarm — Build & Deploy

## First-Time Flash (per device, USB serial)

### Prerequisites
- ESPHome CLI installed and on PATH
- Device connected via USB
- COM port identified (Device Manager → Ports)

### Steps

```powershell
cd c:\Work\esphome
esphome run config\air-alarm.yaml --device COM3
```

Replace `COM3` with the actual port. This compiles the firmware, flashes it over serial, and opens the log monitor. Press `Ctrl+C` to exit the monitor.

After first boot the device:
1. Connects to WiFi
2. Appears in HA under its MAC-suffixed name (e.g. `air-alarm-a1b2c3`)
3. Sits unprovisioned — no sensors are advertised yet

### Provisioning

In HA → Developer Tools → Services, call:

```yaml
service: esphome.air_alarm_a1b2c3_provision
data:
  device_id: "air-alarm"
  device_num: 1
  location: "Living Room"
```

The device stores the identity in NVS (persists across reboots), publishes MQTT discovery, and sensors appear in HA as:
- `sensor.air_alarm_1_alarm_event_activity`
- `sensor.air_alarm_1_alarm_event_time`

Repeat for each device, incrementing `device_num` (1–4).

---

## OTA Updates (all subsequent flashes)

### Steps

```powershell
cd c:\Work\esphome
.\config\deploy.ps1
```

The script:
1. Reads the version from `git describe --tags --long --dirty`
2. Compiles `config\air-alarm.yaml`
3. Computes the MD5 of the binary
4. Copies `air-alarm.bin`, `air-alarm.md5`, and `air-alarm-version.txt` to `\\192.168.4.2\config\www\esphome\`

Then in HA, go to each device's page and press **Update Firmware**. The device:
1. Downloads `air-alarm.bin` from `http://192.168.4.2:8123/local/esphome/air-alarm.bin`
2. Verifies against `air-alarm.md5`
3. Flashes and reboots

### Checking what's deployed

```
http://192.168.4.2:8123/local/esphome/air-alarm-version.txt
```

Returns the git-derived version string, e.g. `v1.2.0_3_gabcdef1`.

### Version string format

| Git state | Version string |
|---|---|
| Exactly on tag `v1.2.0`, clean | `v1.2.0_0_gabcdef1` |
| 3 commits past tag, clean | `v1.2.0_3_gabcdef1` |
| Uncommitted changes present | `v1.2.0_3_gabcdef1_dirty` |
| No tags in repo | `unknown` |

---

## Status LED Reference

The NeoPixel on each Feather board shows device state at a glance.

| Colour | Pattern | Meaning |
|---|---|---|
| **Orange** | Solid | Unprovisioned — device has no identity yet. Call the `provision` service. |
| **Cyan** | Solid (fades in) | Idle — provisioned and connected to WiFi/MQTT. Normal operating state. |
| **Green** | 300ms burst → cyan | Door **opened** (positive pressure spike detected). One flash per event. |
| **Blue** | 300ms burst → cyan | Door **closed** (negative pressure spike detected). One flash per event. |
| **Purple** | Solid | OTA firmware update started. Device will reboot when complete. |
| **Red** | 500ms on / 500ms off | SNTP heartbeat — pulses in sync with wall-clock seconds across all devices. Used to visually verify time synchronisation between units. |

### LED behaviour notes

- The red LED and the NeoPixel are independent: the red LED always runs the SNTP heartbeat when WiFi is connected; the NeoPixel shows the above states.
- Event flashes are edge-triggered — one flash fires when the pressure event starts, not once per sample. A door event lasting several seconds still produces exactly one coloured burst.
- The 2-second idle refresh maintains orange/cyan as provisioning or WiFi state changes, but never interrupts an event flash or OTA purple.
- Orange → cyan transition happens automatically on the `provision` service call; no reboot required.

---

## Logs & Crash Diagnostics

### Live logs (WiFi, no USB needed)

```powershell
esphome logs config\air-alarm.yaml
```

Connects over mDNS/OTA and streams the serial log to the terminal. Press `Ctrl+C` to stop. Most useful for watching a device in real time after a reboot.

### Live logs from HA

Settings → Devices → [device] → **Logs** button. Streams the same log over the native API. Not persisted — only shows output since the page was opened.

### Reset Reason (persists across reboots)

Each device exposes a **Reset Reason** diagnostic entity in HA (Settings → Devices → [device]). Common values:

| Value | Meaning |
|---|---|
| `Power On` | Normal first boot |
| `Software` | Intentional restart (button, OTA) |
| `Watchdog` | Main loop blocked >5 s — check MQTT/WiFi stalls |
| `Brownout` | Supply voltage dropped — check power supply |
| `Exception/Panic` | Firmware crash — needs serial backtrace |

### MQTT log stream (WARN+ messages, persisted in broker)

All devices publish WARN-level and above log messages to MQTT when provisioned:

```
Topic:   esphome/<device-id>-<device-num>/log
Example: esphome/air-alarm-1/log
```

Subscribe to all devices at once in MQTT Explorer:

```
esphome/+/log
```

This captures watchdog warnings, SNTP failures, and any `ESP_LOGW` / `ESP_LOGE` calls without needing USB. Messages survive HA restarts (broker retains the last value).

### Serial backtrace (Exception/Panic crashes)

If Reset Reason shows `Exception/Panic`, connect USB and run:

```powershell
esphome logs config\air-alarm.yaml --device COM3
```

The backtrace printed to serial can be decoded with the ESP-IDF monitor tool or pasted into [https://espressi.f.io/](https://github.com/espressif/esp-idf-monitor) (offline tool in the ESP-IDF toolchain).

---

## Re-provisioning

To change a device's identity, call the provision service again with new values. To fully reset:

```yaml
service: esphome.air_alarm_a1b2c3_deprovision
```

This clears NVS and removes sensors from HA on the next MQTT reconnect.
