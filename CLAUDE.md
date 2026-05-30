# Fly Controller — Claude Code Guide

ESP32-C3 Arduino/PlatformIO firmware for intelligent drone/UAV flight control. Supports three ESC types via three build targets.

## Build System

**Platform:** PlatformIO · **Board:** `lolin_c3_mini` (ESP32-C3) · **Framework:** Arduino

```bash
# Build
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag

# Upload
~/.platformio/penv/bin/pio run -e <env> -t upload

# Serial monitor
~/.platformio/penv/bin/pio device monitor -e <env>
```

Do NOT run `pio` without the full path — it may not be in PATH.

## Releases

Pushing a git tag triggers `.github/workflows/build-and-release.yml`, which builds all three targets and publishes a GitHub Release with the firmware binaries. Tags follow the `YYYY-MM-DD.N` convention (e.g. `2026-05-29.1`, where `N` increments for multiple releases on the same day). The tag becomes `APP_VERSION` (via generated `src/Version.h`) and the release-notes changelog spans from the previous tag (resolved via `git describe` on the tagged commit's parent) to the new tag.

## Build Targets

| Environment | `CONTROLLER_TYPE` | Macro | CAN Bus | Protocol |
|---|---|---|---|---|
| `lolin_c3_mini_hobbywing` | 2 | `IS_HOBBYWING` | 500 kbps | DroneCAN |
| `lolin_c3_mini_tmotor` | 3 | `IS_TMOTOR` | 1 Mbps | UAVCAN |
| `lolin_c3_mini_xag` | 1 | `IS_XAG` | None | PWM-only |

`config_controller.h` derives `IS_HOBBYWING` / `IS_TMOTOR` / `IS_XAG` / `USES_CAN_BUS` from `CONTROLLER_TYPE`. Always use `#if IS_HOBBYWING` (not `#ifdef`).

## Project Structure

```
src/
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
1. Create `src/ComponentName/ComponentName.h` and `.cpp`
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

- **Language:** All code and comments in English
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
