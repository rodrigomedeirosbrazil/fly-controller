# Fly Controller — Claude Code Guide

ESP32-C3 Arduino/PlatformIO firmware for intelligent drone/UAV flight control. Supports three ESC types via three build targets.

This is a monorepo. The controller firmware lives in `controller/` (its `platformio.ini` is there); the remote-throttle firmware lives in `throttle/`; `shared/` holds code used by both (e.g. the ESP-NOW protocol header). Run all `pio` commands below from `controller/`.

## Build System

**Platform:** PlatformIO · **Board:** `lolin_c3_mini` (ESP32-C3) · **Framework:** Arduino

```bash
# Build (run from controller/)
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag

# Upload
~/.platformio/penv/bin/pio run -e <env> -t upload

# Serial monitor
~/.platformio/penv/bin/pio device monitor -e <env>

# Remote wireless throttle firmware (run from throttle/)
~/.platformio/penv/bin/pio run -e remote_throttle
```

Do NOT run `pio` without the full path — it may not be in PATH.

## Releases

Pushing a git tag triggers `.github/workflows/build-and-release.yml`, which builds all three controller targets plus the remote-throttle firmware and publishes a GitHub Release with the firmware binaries. Tags follow the `YYYY-MM-DD.N` convention (e.g. `2026-05-29.1`, where `N` increments for multiple releases on the same day). The tag becomes `APP_VERSION` (via generated `controller/src/Version.h`) and the release-notes changelog spans from the previous tag (resolved via `git describe` on the tagged commit's parent) to the new tag.

## Build Targets

| Environment | `CONTROLLER_TYPE` | Macro | CAN Bus | Protocol |
|---|---|---|---|---|
| `lolin_c3_mini_hobbywing` | 2 | `IS_HOBBYWING` | 500 kbps | DroneCAN |
| `lolin_c3_mini_tmotor` | 3 | `IS_TMOTOR` | 1 Mbps | UAVCAN |
| `lolin_c3_mini_xag` | 1 | `IS_XAG` | None | PWM-only |

`config_controller.h` derives `IS_HOBBYWING` / `IS_TMOTOR` / `IS_XAG` / `USES_CAN_BUS` from `CONTROLLER_TYPE`. Always use `#if IS_HOBBYWING` (not `#ifdef`).

## Project Structure

Monorepo layout: `controller/` (the controller firmware, tree below), `throttle/` (remote wireless-throttle firmware), and `shared/` (`RemoteLinkProtocol.h`, included by both).

```
controller/src/
├── main.cpp / main.h         # setup() + loop(); routes CAN frames, calls all components
├── config.h / config.cpp     # All extern declarations + global object instantiations
├── config_controller.h       # Build-type macros derived from CONTROLLER_TYPE
├── ADS1115/                  # I2C ADC (Hobbywing/Tmotor: throttle + motor temp)
├── BatteryMonitor/           # Coulomb counting + SoC from voltage
├── BluetoothBms/             # BLE BMS integration
├── Button/                   # AceButton single-click (arm) + long-press (cruise)
├── Buzzer/                   # Non-blocking PWM beep patterns
├── Canbus/                   # TWAI receive() — returns raw frames to main.cpp
├── DalyBms/                  # Daly BMS serial protocol
├── Hobbywing/                # HobbywingCan + HobbywingTelemetry
├── JbdBms/                   # JBD BMS serial protocol
├── Logger/                   # CSV logging to LittleFS
├── Power/                    # Available-power calculation + throttle ramp limiting
├── RemoteLink/               # ESP-NOW remote-throttle link + host-tested failsafe logic
├── Sensors/                  # BatteryVoltageSensor (XAG voltage divider)
├── Settings/                 # Persistent config via ESP32 Preferences
├── Telemetry/                # Facade (delegates to *Telemetry) + TelemetryData struct
├── Temperature/              # NTC thermistor via ReadFn
├── Throttle/                 # Hall sensor via ReadFn + calibration + cruise
├── Tmotor/                   # TmotorCan + TmotorTelemetry
├── WebServer/                # ESPAsyncWebServer + ElegantOTA config portal
│   └── Pages/                # HTML/JS page handlers
├── Xag/                      # XagTelemetry (analog sensors, PWM-only)
└── Xctod/                    # BLE telemetry output for XCTRACK app
```

## Key Patterns

### Adding a New Component
1. Create `controller/src/ComponentName/ComponentName.h` and `.cpp`
2. Add `extern ComponentName componentName;` to `config.h`
3. Instantiate in `config.cpp`
4. Call `componentName.setup()` in `setup()` and `componentName.handle()` in `loop()`

### ReadFn Pattern
`Temperature` and `Throttle` accept a `ReadFn` (function pointer) for ADC reading — either `ADS1115` or `analogRead`. This keeps sensor logic hardware-agnostic.

### Telemetry Facade
`telemetry.getXxx()` delegates to `HobbywingTelemetry` / `TmotorTelemetry` / `XagTelemetry` based on build target. Always use the facade — never access ESC objects directly for telemetry.

### CAN Bus Routing
`Canbus::receive()` is non-blocking and returns raw frames. `main.cpp` routes them:
```cpp
while (canbus.receive(&msg)) {
    hobbywingCan.parseEscMessage(msg); // or tmotorCan
}
```
`Canbus` handles NodeStatus/GetNodeInfo internally.

## Coding Conventions

- **Language:** All code, comments, commit messages, documentation, and identifiers in **English**. The only exception is user-facing strings rendered in the web portal and UI (button labels, page text, error messages shown to the user) — those are in **Brazilian Portuguese**.
- **No `delay()`** in main loop — use `millis()` for timing
- **Naming:** camelCase for functions/variables, PascalCase for classes
- **Constants:** `#define` or `const` — no magic numbers
- **Build guards:** `#if IS_HOBBYWING` not `#ifdef IS_HOBBYWING`

## Pin Assignments

| GPIO | Function | Notes |
|---|---|---|
| 0 | Throttle (Hall sensor) | ADC / ADS1115 ch0 |
| 1 | Motor temperature (NTC) | ADC / ADS1115 ch1 |
| 2 | CAN TX (TWAI) | Hobbywing/Tmotor only |
| 3 | CAN RX (TWAI) / Battery voltage | XAG: voltage divider |
| 4 | ESC temperature (NTC) | XAG only |
| 5 | Button | AceButton, interrupt |
| 6 | Buzzer | PWM, passive piezo |
| 7 | ESC PWM output | 1050–1950 μs |

## Power & Safety Logic

- **Battery voltage:** Progressive reduction below nominal (configurable via Settings)
- **Motor temp:** Linear reduction 50°C → 60°C
- **ESC temp:** Linear reduction 80°C → 110°C
- **Throttle ramp:** `THROTTLE_RAMP_UP_US_PER_MS` (accel) / `THROTTLE_RAMP_DOWN_US_PER_MS` (decel)
- **XAG wake-up:** 1.5 s at 5% PWM before ramp when starting from stopped (`XAG_MOTOR_REACTION_DELAY_MS`)

## Settings (Persistent)

All tunable parameters live in `Settings/` and are stored via `ESP32 Preferences`. Changes survive reboots. Configurable via the web portal (`WebServer/`).

## Web Portal

Available on all builds. Connects to WiFi AP, serves config pages at `192.168.4.1`. OTA firmware update via ElegantOTA.

WiFi is enabled at boot and stays on for the whole session (ESP-NOW shares the radio and must not be torn down). TX power is pinned to 8.5 dBm — the ESP32-C3 Supermini is unstable at full power (commit f06aa0d).

The **telemetry page** (`/telemetry`) polls `/api/telemetry` every 1 s and mirrors buzzer beeps in the browser via Web Audio API. The `buzzer` field in the JSON response (`BeepEvent` snapshot: `seq`, `freq`, `onMs`, `offMs`, `reps`, `active`) drives a square-wave oscillator player. A 🔔/🔇 toggle button in the status bar unlocks the `AudioContext` (browser autoplay policy) and controls mute.

## Wireless Throttle (ESP-NOW)

An optional second ESP32 (the **remote throttle**, firmware in `throttle/`) reads a Hall sensor + button and sends them to the controller over **ESP-NOW** (channel 1, coexisting with the WiFi AP + BLE on the C3's single radio). The wire contract is `shared/RemoteLinkProtocol.h`, included by both firmwares.

- **Source selection:** `Settings::getThrottleSource()` (wired/wireless), set in the web portal. In wireless mode the controller's `Throttle` `ReadFn` returns the last Hall value received over the link, and the existing AceButton runs on the remote's forwarded **raw button state** (via `SourceSwitchButtonConfig::readButton`) — so calibration and the arming gesture are identical wired/wireless. The remote stays "dumb"; the controller owns all logic.
- **Failsafe (`RemoteLink/RemoteLinkLogic.h`, host-tested):** wireless mode only. No packet for >500 ms → ramp throttle to 0 (stay armed, via the ReadFn feeding 0); >3 s → disarm.
- **Pairing:** web portal "Parear remote" arms pairing; the next remote heard is saved (MAC in `Settings`). The remote persists the controller MAC in NVS and enters pairing by holding its button while unpaired.
- **Component:** `controller/src/RemoteLink/` (ESP-NOW transport, beep forwarding, pairing). Both buzzers stay active — key beeps are forwarded to the remote via `remoteLink.requestBeep()`.

Remote pinout (ESP32-C3 Supermini): Hall=GPIO0, button=GPIO5, buzzer=GPIO6, red LED=GPIO7 (armed), green LED=GPIO10 (disarmed). Pure decision logic (LED state machine, link-loss, failsafe) lives in host-testable headers tested with `c++ -std=c++17` like `test/PowerTest.cpp`.
