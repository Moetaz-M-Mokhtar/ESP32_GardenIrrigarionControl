# ESP32 Garden Irrigation Control

[![Platform: ESP32](https://img.shields.io/badge/platform-ESP32-orange.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Framework: Arduino](https://img.shields.io/badge/framework-Arduino-brightgreen.svg)](https://docs.platformio.org/en/latest/frameworks/arduino.html)
[![Build: PlatformIO](https://img.shields.io/badge/build-PlatformIO-blue.svg)](https://platformio.org)
[![BLE: NimBLE](https://img.shields.io/badge/BLE-NimBLE-legacy)](https://github.com/h2zero/NimBLE-Arduino)
[![License: AGPL-3.0](https://img.shields.io/badge/license-AGPL--3.0-blue.svg)](LICENSE)

A low-power, BLE-controlled **smart garden irrigation controller** built on the ESP32. It drives up to 4 solenoid valve drivers on a **latching SN7475N** circuit, keeps time with a **DS3231 RTC**, compensates for clock drift in software, and wakes from **deep sleep** only when watering is actually due.

Configured and monitored over **Bluetooth Low Energy** from a companion Flutter app — no cloud, no Wi-Fi, no account.

---

## Table of Contents

- [Features](#features)
- [Hardware](#hardware)
- [Architecture](#architecture)
- [How It Works](#how-it-works)
- [BLE Protocol](#ble-protocol)
- [Companion App](#companion-app)
- [Clock Drift Compensation](#clock-drift-compensation)
- [Deep Sleep & Power Management](#deep-sleep--power-management)
- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Development Guides](#development-guides)
- [License](#license)

---

## Features

| Feature | Description |
|---------|-------------|
| **4 valve drivers** | Latching SN7475N solenoid drivers (2 latched pairs) |
| **15 schedules** | Programmable start time, duration, days-of-week (bitmask), and zone bitmask per schedule |
| **BLE control** | Full configuration, manual forcing, and monitoring over Bluetooth Low Energy |
| **Time sync** | One-tap clock sync from the phone over BLE |
| **Clock drift compensation** | Software-corrected time — no need to re-sync the RTC every month |
| **Deep sleep** | RTC wakes the ESP32 only for scheduled events; GPIO hold keeps valve states latched through sleep |
| **Force mode** | Instantly open/close a valve for a fixed duration, overriding schedules |
| **Error manager** | Severity-based error tracking with function inhibition (AUTOSAR-style) |
| **Remote debug stream** | BLE characteristic streaming live diagnostic data, like a wireless serial monitor |
| **NVS persistence** | Schedules and drift calibration survive deep sleep and power loss |
| **Heartbeat supervision** | Disconnects an unresponsive client after 30 s |

---

## Hardware

### Required Components

| Component | Purpose |
|-----------|---------|
| ESP32 DevKit (esp32dev) | Main controller |
| DS3231 RTC module (with battery) | Accurate timekeeping |
| SN7475N latch (×2) | Latching solenoid drivers |
| ULN2003 / MOSFET drivers (×4) | Driving the solenoids |
| 24 V (or 12 V) solenoid valves (×4) | Water-flow control |
| 5 V / 3.3 V power supplies | Logic + solenoid power |
| BLE-capable phone | Companion Flutter app |

### GPIO Map

| ESP32 Pin | Function | Notes |
|-----------|----------|-------|
| GPIO 13 | Data line D1/D3 | Shares the SN7475N data input |
| GPIO 15 | Data line D2/D4 | Shares the SN7475N data input |
| GPIO 26 | Enable E12 (latch driver 0 + 1) | Latched pair |
| GPIO 27 | Enable E34 (latch driver 2 + 3) | Standalone pair |
| GPIO 4 | DS3231 SQW interrupt | Deep-sleep wakeup |

### Driver Wiring (drivers defined in `HwAbstr.cpp`)

| Driver | Drive pin | Enable pin | Type |
|--------|-----------|------------|------|
| 0 | GPIO 13 | GPIO 26 | Latch SN7475N (coupled with 1) |
| 1 | GPIO 15 | GPIO 26 | Latch SN7475N (coupled with 0) |
| 2 | GPIO 13 | GPIO 27 | Latch SN7475N (coupled with 3) |
| 3 | GPIO 15 | GPIO 27 | Latch SN7475N (coupled with 2) |

> The SN7475N latch keeps the output state after the pulse, so the ESP32 GPIOs are only *triggers*. During deep sleep the pins are held low (latch mode) via `gpio_hold_en` — the latch preserves the valve state.

---

## Architecture

An AUTOSAR-inspired modular design. Every module has a single responsibility and communicates through well-defined interfaces.

| Module | Files | Responsibility |
|--------|-------|----------------|
| **ErrM** | `ErrM.cpp/hpp` | Error tracking + severity-based function inhibition |
| **CfgM** | `CfgM.cpp/hpp` | Schedule storage, validation, NVS persistence |
| **TimerCtrl** | `TimerCtrl.cpp/hpp` | DS3231 RTC wrapper, alarms, temperature readback |
| **ClockDrift** | `ClockDrift.cpp/hpp` | Software drift correction, calibration, NVS persistence |
| **Scheduler** | `Scheduler.cpp/hpp` | Time-based scheduling logic — decides *what should be on* |
| **HwAbstr** | `HwAbstr.cpp/hpp` | Hardware abstraction — the *only* module that touches GPIOs |
| **BleComm** | `BleComm.cpp/hpp` | NimBLE server, JSON protocol, heartbeat supervision |
| **DebugM** | `DebugM.cpp/hpp` | Diagnostic stream formatting |

```
┌──────────────────────────────────────────────────────────┐
│                         main.cpp                          │
│   wires modules, runs pairing window, manages deep sleep  │
└──────────────┬──────────────────────────┬─────────────────┘
               │                          │
      ┌────────▼────────┐         ┌────────▼────────┐
      │  BleComm (BLE)  │         │  Scheduler      │
      │  NimBLE server   │         │  time planning  │
      └────────┬────────┘         └────────┬────────┘
               │                            │
      ┌────────▼────────┐         ┌────────▼────────┐
      │  CfgM (NVS)     │         │  HwAbstr (GPIO) │
      │  schedules       │         │  SN7475N latch  │
      └────────┬────────┘         └────────┬────────┘
               │                            │
      ┌────────▼────────┐         ┌────────▼────────┐
      │ ClockDrift      │         │ Driver 0..3     │
      │  software time  │         │  (solenoids)    │
      └────────┬────────┘         └─────────────────┘
               │
      ┌────────▼────────┐
      │ TimerCtrl (RTC) │
      │  DS3231 alarms  │
      └─────────────────┘
```

**Key design rules** (enforced by the development process):

- **Nothing but HwAbstr touches GPIOs** — all hardware writes go through `set_HwState()`.
- **Force always wins** — a forced valve stays on even when a schedule says off, until its timer expires.
- **Scheduler owns decisions, BleComm owns protocol, CfgM owns persistence** — no cross-module shortcuts.

---

## How It Works

### Automatic Mode (no phone connected)

1. ESP32 runs the scheduler, then enters **deep sleep** to save power.
2. The DS3231 RTC triggers an interrupt on GPIO 4 when the *next* relevant time arrives (start or completion of a schedule).
3. ESP32 wakes, re-arms the alarm for the next event, applies valve changes, and sleeps again.
4. A **fallback alarm** (3 h after the last event) guarantees wakeup even if a planned event is missed.

### BLE Manual Mode (phone connected)

1. ESP32 advertises as **SprinkCtrl**, with a 30 s pairing window after boot.
2. The app connects, stays alive by sending a heartbeat every 5 s, and can:
   - sync the clock,
   - read/edit schedules,
   - force valves open or closed,
   - read live status and an optional debug stream.
3. If no heartbeat arrives for 30 s, ESP32 disconnects the client and returns to automatic mode.

---

## BLE Protocol

**Service UUID:** `12345678-1234-5678-1234-56789ABCDEF0`
**Device name:** `SprinkCtrl`

| Characteristic | UUID | Properties | Purpose |
|----------------|------|------------|---------|
| **TimeSync** | `ABCDEF00-…` | Read / Write | `{"ts": <unix_timestamp>}` |
| **Config** | `ABCDEF01-…` | Read / Write | Schedules + actions (see below) |
| **Status** | `ABCDEF02-…` | Read / Notify | Live status JSON |
| **Heartbeat** | `ABCDEF04-…` | Write | `{}` keep-alive |
| **Schedules** | `ABCDEF06-…` | Read / Notify | Flat array of schedules |
| **ForceValve** | `ABCDEF07-…` | Read / Write | Force a valve open/closed |
| **DebugStream** | `ABCDEF08-…` | Read / Write / Notify | Live diagnostics |

### Status Payload (notify every 1 s while connected)

```json
{
  "unix": 1789497000,
  "temp": 24.5,
  "boot": 12,
  "drift_ppm": 125.0,
  "last_sync": 1789392000,
  "mode": "automatic",
  "valves": [1, 0, 1, 0],
  "forced": [0, 0, 1, 0],
  "timer_start": [0, 0, 1789497090, 0],
  "timer_duration": [0, 0, 1800, 0],
  "errors": [0, 1, 0, 0, 0]
}
```

### Config Actions (write to `Config`)

| Payload | Effect |
|---------|--------|
| `{"action": "read_schedules"}` | ESP32 re-serializes the Schedules characteristic |
| `{"action": "reset_drift"}` | Resets the drift coefficient to 0 |
| `{"schedule": {"id": 0, "h": 7, "m": 0, "period": 30, "dow": 124, "zones": 3, "driver": 0}}` | Create/update a schedule |
| `{"schedule": {"id": 2, "h": 0, "m": 0, "period": 30, "dow": 0, "zones": 0}}` | Delete a schedule (`dow = 0` = free slot) |

### Schedules Payload (read from `Schedules`)

```json
{"s": [[id, h, m, period_min, dow_bitmask, zones_bitmask, driver_idx], ...]}
```

- `period` is in **minutes** (uint16, up to 65535).
- `dow`: bit 0 = Monday … bit 6 = Sunday (e.g. `124` = Mon–Fri).
- `zones`: bitmask over the 4 drivers.

### ForceValve Payload (write to `ForceValve`)

```json
{"valve": 2, "force": true, "state": 1, "duration": 3600}
{"valve": 2, "force": false, "state": 0}
```

> Force states persist across BLE reconnects by design — if you force a valve on and close the app, it stays on until the timer expires.

---

## Companion App

Control panel is a separate **Flutter** project — [**GardenIrrigationRemote**](https://github.com/Moetaz-M-Mokhtar/GardenIrrigationRemote) — providing:

- BLE scanning, connecting, and reconnecting with heartbeat supervision
- Dashboard with live status, countdown timers, and drift gauge
- Schedule editor (time, duration, days, zones)
- Manual valve forcing with per-driver enable/disable
- One-tap time sync with drift visualization
- Remote debug stream viewer

The two projects are tightly coupled: the UUIDs above are mirrored in the app's [`BleConstants`](https://github.com/Moetaz-M-Mokhtar/GardenIrrigationRemote/blob/main/lib/core/constants/ble_constants.dart).

---

## Clock Drift Compensation

The DS3231 runs at its own rate, and cheap modules often drift (this project's unit measures ~694 PPM ≈ 30 min/month). Instead of syncing the RTC constantly, the firmware keeps the **physical RTC as the raw clock** and applies a mathematical correction in software.

### Model

| Parameter | Meaning |
|-----------|---------|
| `R` | Raw RTC time (DS3231, uncorrected) |
| `T` | Corrected application time |
| `T₀` | Phone unixtime at last sync |
| `P` | Drift coefficient in PPM (`P > 0` = RTC runs fast) |
| `k` | `P × 10⁻⁶` |

**Corrected time:** `T = R − k × (R − T₀)`

**Alarm conversion (corrected → raw RTC):** `R_alarm = (T_alarm − k × T₀) / (1 − k)`

**Calibration update at each sync:** `P_new = P_old + e / Δt × 10⁶`, where `e = T_corrected − T_phone`.

### Calibration Rules

- Only accept a calibration sample if:
  - elapsed since last sync ≥ **18 hours**, and
  - `|e| ≤ 30 minutes`.
- Rejected samples: RTC is still synced, previous drift estimate persists.
- Per-sync drift step is clamped to **±500 PPM**; coefficient is capped at **3000 PPM**.
- Calibration parameters live in NVS (`namespace: clock_drift`) — no exponential smoothing, incremental correction only.

---

## Deep Sleep & Power Management

| Mechanism | Description |
|-----------|-------------|
| **Deep sleep** | ESP32 sleeps between events; typical quiescent current is in the µA range (plus DS3231) |
| **RTC interrupt** | DS3231 SQW pin (GPIO 4) wakes the ESP32 exactly when needed |
| **GPIO hold** | Data + enable pins held low (`gpio_hold_en` + `gpio_deep_sleep_hold_en`) → SN7475N stays in latch mode, valve states preserved through sleep |
| **Release on wake** | `HwAbstr_Init()` releases the holds before any GPIO writes |
| **Fallback alarm** | A backup RTC alarm 3 h after the last event prevents missing a schedule |
| **Force keeps awake** | While any valve is forced, the device stays awake to enforce it, then sleeps when all timers expire |

---

## Getting Started

### Prerequisites

- [PlatformIO Core](https://platformio.org/install) (CLI or the VS Code extension)
- Python ≥ 3.8 (bundled with PlatformIO)
- ESP32 DevKit connected via USB (USB-UART driver installed)

### Build

```bash
pio run
```

### Upload to device

```bash
pio run -t upload
```

or with an explicit port:

```bash
pio run -t upload --upload-port /dev/ttyUSB0
```

### Serial monitor (debug)

```bash
pio device monitor
```

> The device normally sleeps; press reset (pulse RTS) while monitoring to catch the boot sequence and 10-second diagnostic dumps.

### Configuration

All build options and dependencies live in [`platformio.ini`](platformio.ini). Modify the GPIO map and driver wiring in `src/HwAbstr.cpp` and the schedule defaults in `src/Scheduler.cpp`.

---

## Project Structure

```
.
├── include/            # Module headers (single responsibility per module)
│   ├── BleComm.hpp     #   BLE protocol definitions & UUIDs
│   ├── CfgM.hpp        #   Schedule management
│   ├── ClockDrift.hpp  #   Drift compensation
│   ├── ErrM.hpp        #   Error manager
│   ├── HwAbstr.hpp     #   Hardware abstraction (GPIOs)
│   ├── Scheduler.hpp   #   Scheduling logic
│   └── TimerCtrl.hpp   #   DS3231 RTC wrapper
├── src/                # Module implementations + main.cpp
├── test/               # PlatformIO tests
├── lib/                # Project-local libraries (empty — deps via PlatformIO)
└── platformio.ini      # Build configuration
```

### Module RX/TX Rules (dependencies)

- **ErrM** — no dependencies; leaf module.
- **HwAbstr** — depends on ErrM; sole owner of GPIO writes.
- **TimerCtrl** — wraps DS3231/RTC; no outbound calls.
- **ClockDrift** — wraps TimerCtrl time into corrected time.
- **Scheduler** — uses TimerCtrl + HwAbstr; decides requested states.
- **CfgM** — pure data; no outbound calls.
- **BleComm** — binds everything to BLE; defers I2C/GPIO work to the main loop.

---

## License

[GNU AGPL-3.0](LICENSE) — see the [LICENSE](LICENSE) file for details.

---

Made with 🌱 for reliably watered gardens.