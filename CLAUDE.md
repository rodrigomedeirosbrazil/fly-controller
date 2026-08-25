# Fly Controller — Claude Code Guide

ESP32-C3 Arduino/PlatformIO firmware for intelligent drone/UAV flight control. Supports two ESC types via two build targets.

The wireless remote-throttle firmware that pairs with this controller lives in a separate
repo, [fly-throttle](https://github.com/rodrigomedeirosbrazil/fly-throttle).

## Build System

**Platform:** PlatformIO · **Board:** `lolin_c3_mini` (ESP32-C3) · **Framework:** Arduino

```bash
# Build
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag

# Upload
~/.platformio/penv/bin/pio run -e <env> -t upload

# Serial monitor
~/.platformio/penv/bin/pio device monitor -e <env>
```

Do NOT run `pio` without the full path — it may not be in PATH.

## Releases

Pushing a git tag triggers `.github/workflows/build-and-release.yml`, which builds both controller targets and publishes a GitHub Release with the firmware binaries. Tags follow the `YYYY-MM-DD.N` convention (e.g. `2026-05-29.1`, where `N` increments for multiple releases on the same day). The tag becomes `APP_VERSION` (via generated `src/Version.h`) and the release-notes changelog spans from the previous tag (resolved via `git describe` on the tagged commit's parent) to the new tag.

## Build Targets

| Environment | `CONTROLLER_TYPE` | Macro | CAN Bus | Protocol |
|---|---|---|---|---|
| `lolin_c3_mini_tmotor` (default) | 3 | `IS_TMOTOR` | 1 Mbps | UAVCAN |
| `lolin_c3_mini_xag` | 1 | `IS_XAG` | None | PWM-only |

`config_controller.h` derives `IS_TMOTOR` / `IS_XAG` / `USES_CAN_BUS` from `CONTROLLER_TYPE`. Always use `#if IS_TMOTOR` (not `#ifdef`). (`CONTROLLER_TYPE=2` is a retired Hobbywing value — recoverable from pre-removal git tags if ever needed.)

## Project Structure

```
src/
├── main.cpp / main.h         # setup() + loop(); routes CAN frames, calls all components
├── config.h / config.cpp     # All extern declarations + global object instantiations
├── config_controller.h       # Build-type macros derived from CONTROLLER_TYPE
├── ADS1115/                  # I2C ADC (Tmotor: throttle + motor temp)
├── BatteryMonitor/           # Coulomb counting + SoC from voltage
├── BluetoothBms/             # BLE BMS integration
├── Button/                   # AceButton single-click (arm) + long-press (cruise)
├── Buzzer/                   # LEDC PWM tone driver (hardware only)
├── Canbus/                   # TWAI receive() — returns raw frames to main.cpp
├── DalyBms/                  # Daly BMS (BLE) protocol
├── JbdBms/                   # JBD BMS (BLE) protocol
├── JkBms/                    # JK BMS (BLE, JK02) protocol + frame parser
├── Logger/                   # CSV logging to LittleFS
├── Power/                    # Available-power calculation + throttle ramp limiting
├── RemoteLink/               # ESP-NOW remote-throttle link + host-tested failsafe logic
├── Sensors/                  # BatteryVoltageSensor (XAG voltage divider)
├── Settings/                 # Persistent config via ESP32 Preferences
├── Sound/                    # Layered audio policy: event queue + persistent state
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
`telemetry.getXxx()` delegates to `TmotorTelemetry` / `XagTelemetry` based on build target. Always use the facade — never access ESC objects directly for telemetry.

### CAN Bus Routing
`Canbus::receive()` is non-blocking and returns raw frames. `main.cpp` routes them:
```cpp
while (canbus.receive(&msg)) {
    tmotorCan.parseEscMessage(msg);
}
```
`Canbus` handles NodeStatus/GetNodeInfo internally.

## Agentic Workflow Artifacts

`docs/superpowers/` (specs, plans) and temporary test files (e.g. `docs/*.html`) are **working documents** — never commit them to PRs or feature branches. They are gitignored. Keep them local only.

## Coding Conventions

- **Language:** All code, comments, commit messages, documentation, and identifiers in **English**. The only exception is user-facing strings rendered in the web portal and UI (button labels, page text, error messages shown to the user) — those are in **Brazilian Portuguese**.
- **No `delay()`** in main loop — use `millis()` for timing
- **Naming:** camelCase for functions/variables, PascalCase for classes
- **Constants:** `#define` or `const` — no magic numbers
- **Build guards:** `#if IS_TMOTOR` not `#ifdef IS_TMOTOR`

## Pin Assignments

| GPIO | Function | Notes |
|---|---|---|
| 0 | Throttle (Hall sensor) | ADC / ADS1115 ch0 |
| 1 | Motor temperature (NTC) | ADC / ADS1115 ch1 |
| 2 | CAN TX (TWAI) | Tmotor only |
| 3 | CAN RX (TWAI) / Battery voltage | XAG: voltage divider |
| 4 | ESC temperature (NTC) | XAG only |
| 5 | Button | AceButton, interrupt |
| 6 | Buzzer | PWM, passive piezo |
| 7 | ESC PWM output | 1050–1950 μs |

## Power & Safety Logic

- **Battery voltage:** Progressive reduction below nominal (configurable via Settings)
- **Motor temp:** Linear reduction 50°C → 60°C
- **ESC temp:** Linear reduction 80°C → 110°C
- **Throttle engage gate:** motor output stays at `ESC_MIN_PWM` until the filtered throttle clears `throttlePinMin + 2%` of the calibrated range, and releases back below `throttlePinMin + 1%` (hysteresis) — see `ThrottleEngagementLogic`. There is no acceleration ramp; PWM tracks the mapped throttle position directly.
- **XAG wake-up:** 1.5 s at 5% PWM before jumping directly to target when starting from stopped (`XAG_MOTOR_REACTION_DELAY_MS`)
- **Sensor validity & arm-time contract:** motor temp, ESC temp, and battery voltage each have a real validity check (`Temperature`/`BatteryVoltageSensor`'s `isValid()`, `SensorReadingValidity`, host-tested) instead of trusting every reading. Arming opens a per-signal contract (`Power::onArmed()`, `SignalArmContract`, host-tested): valid at arm and later invalid mid-flight disarms (`DisarmReason::MotorTempLost`/`EscTempLost`/`BatteryVoltageLost`); invalid at arm disables that signal's limiting for the whole session, even if it recovers. The arm snapshot is taken by the first `Power::checkSignalLoss()` rather than inside `onArmed()` (so it can't straddle two telemetry samples), and the disarm is debounced by `SIGNAL_LOSS_GRACE_MS` (2 s of continuous invalidity) — see `src/CLAUDE.md` under Power for why both are required. Motor temp has two sources behind one reading (CAN vs NTC), so its contract also snapshots the source tag (`telemetry.getMotorTempOrigin()`) on the same tick as validity: a mid-flight source change is treated as loss of the sensor the pilot armed with and disarms with `DisarmReason::MotorTempSourceChanged` (`MOT SRC`) after the same 2 s `SIGNAL_LOSS_GRACE_MS` debounce, and `shouldLimit()` stops the instant the source diverges so one sensor's thresholds are never applied to the other's reading. No sensor valid at arm (or XAG, whose origin is always `None`) → no source-change disarm.

## Settings (Persistent)

All tunable parameters live in `Settings/` and are stored via `ESP32 Preferences`. Changes survive reboots. Configurable via the web portal (`WebServer/`).

## Web Portal

Available on all builds. Connects to WiFi AP, serves config pages at `192.168.4.1`. OTA firmware update via ElegantOTA.

WiFi is enabled at boot and stays on for the whole session (ESP-NOW shares the radio and must not be torn down). TX power is pinned to 8.5 dBm — the ESP32-C3 Supermini is unstable at full power (commit f06aa0d).

The **telemetry page** (`/telemetry`) polls `/api/telemetry` every 1 s and mirrors buzzer sounds in the browser via Web Audio API. The `buzzer` field in the JSON response is an **array** of up to 8 `BeepEvent` entries (ring buffer, oldest→newest: `seq`, `freq`, `onMs`, `offMs`, `reps`, `layer`, `active`) — `layer` is 0 for a queued event, 1 for the persistent state layer. The browser plays fresh queued events (`seq > bzLastSeq`) immediately and toggles a looping oscillator on state transitions only (repeated `active:true`/`active:false` for an already-running/stopped state is ignored); an event pauses the state loop and resumes it afterward. A 🔔/🔇 toggle button in the status bar unlocks the `AudioContext` (browser autoplay policy) and controls mute.

The `/api/telemetry` response also includes a `powerAlert` object (`{seq, causes: [...]}`) driven by the `PowerAlert` component. When any power limiter is active while armed, the telemetry page highlights the affected metric cards in red (persistent while limiting) and shows a dismissible alert panel synced to the `seq` counter (reopens on every 10 s re-fire after dismissal). See `PowerAlert/` for the component and `PowerAlertLogic.h` for the host-testable timing logic.

The `sessionSec` field in `/api/telemetry` is a **flight-time counter** (card "Tempo de vôo" on the Telemetry page) that survives disarm/re-arm cycles within a power cycle: it counts only while `isArmed && motorRunning`, pauses on disarm or motor stop, resumes from the same value, and is RAM-only (resets on boot). Pure decision logic lives in `src/HourMeter/HourMeterLogic.h` (host-tested in `test/HourMeterLogicTest.cpp`); `src/HourMeter/HourMeter.cpp` is the Arduino wrapper that also owns the persistent `hourMeterSec` total (motor-run seconds, saved to NVS). `POST /api/session/reset` (PIN-guarded, like every write route) zeroes the session counter via a deferred flag — the reset is applied on the next `loop()` tick, never from the web-server task. The Telemetry page's reset button prompts for the PIN on a 403 and caches it in `sessionStorage` (`cfgPin`), since `/telemetry` is self-contained and does not include the config pages' PIN field.

## Wireless Throttle (ESP-NOW)

An optional second ESP32 (the **remote throttle**, firmware in the separate [fly-throttle](https://github.com/rodrigomedeirosbrazil/fly-throttle) repo) reads a Hall sensor + button and sends them to the controller over **ESP-NOW** (channel 1, coexisting with the WiFi AP + BLE on the C3's single radio). The wire contract (`src/RemoteLink/RemoteLinkProtocol.h`) is duplicated byte-for-byte in the fly-throttle repo (its `src/RemoteLinkProtocol.h`) — there is no shared package or submodule. Both copies carry a `REMOTE_LINK_PROTOCOL_VERSION` constant; bump it in both repos whenever the wire format changes, and update `test/RemoteLinkProtocolTest.cpp` in both if the packet layout itself changes size.

- **Source selection:** `Settings::getThrottleSource()` (wired/wireless), set in the web portal. In wireless mode the controller's `Throttle` `ReadFn` returns the last Hall value received over the link, and the existing AceButton runs on the remote's forwarded **raw button state** (via `SourceSwitchButtonConfig::readButton`) — so calibration and the arming gesture are identical wired/wireless. The remote stays "dumb"; the controller owns all logic.
- **Signal validity & failsafe (`Throttle/ThrottleSignalLogic.h`, host-tested):** a single fault-escalation state machine shared by both wired and wireless throttle sources, parameterized per source. Wireless: no packet for >500 ms → ramp throttle to 0 (stay armed, via the ReadFn feeding 0); >3 s → disarm. Wired: zero tolerance — any invalid reading (see `Throttle/ThrottleWiredValidity.h`) disarms immediately, no ramp-to-zero step. `RemoteLink::isLinkFresh()` supplies the raw "packet arrived recently" signal that feeds the wireless config; `RemoteLink/RemoteLinkLogic.h` no longer exists (absorbed into `ThrottleSignalLogic`). A disarm from either path is latched as a `DisarmReason` (`src/DisarmReason.h`), surfaced via `/api/telemetry` and shown as a persistent warning on the Telemetry page until the pilot fixes the fault and re-arms.
- **Pairing:** web portal "Parear remote" arms pairing; the next remote heard is saved (MAC in `Settings`). The remote persists the controller MAC in NVS and enters pairing by holding its button while unpaired.
- **Component:** `src/RemoteLink/` (ESP-NOW transport, beep forwarding, pairing; also holds this repo's copy of `RemoteLinkProtocol.h` — see above). Both buzzers stay active — key beeps are forwarded to the remote via `remoteLink.requestBeep()`. The remote's `Armed`/`Stop`/`Disarmed` commands now mirror the controller's own armed+stopped state (`updateSoundState()` in `main.cpp`, using the same `throttle.isEngaged()` hysteresis as `Sound`'s `ArmedIdle` state) — the two used to disagree, with the remote beeping on armed alone.

Remote pinout (ESP32-C3 Supermini): Hall=GPIO0, button=GPIO5, buzzer=GPIO6, red LED=GPIO7 (armed), green LED=GPIO10 (disarmed). Pure decision logic (LED state machine, link-loss, failsafe) lives in host-testable headers tested with `c++ -std=c++17` like `test/PowerTest.cpp`.
