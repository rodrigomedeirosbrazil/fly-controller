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
├── Logger/                   # CSV logging to LittleFS + LogListing (pure) / LogStore (LittleFS) for BLE log access
├── Power/                    # Available-power calculation + throttle ramp limiting
├── RemoteLink/               # ESP-NOW remote-throttle link + host-tested failsafe logic
├── Sensors/                  # BatteryVoltageSensor (XAG voltage divider)
├── Settings/                 # Persistent config via ESP32 Preferences
├── Sound/                    # Layered audio policy: event queue + persistent state
├── Telemetry/                # Facade (delegates to *Telemetry) + TelemetryData struct
├── Temperature/              # NTC thermistor via ReadFn
├── Throttle/                 # Hall sensor via ReadFn + calibration + cruise
├── Tmotor/                   # TmotorCan + TmotorTelemetry
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

- **Language:** All code, comments, commit messages, documentation, and identifiers in **English**. The only exception is the user-facing manuals (`docs/MANUAL-*.md`), which are in **Brazilian Portuguese**.
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
- **Motor temp:** Linear reduction between two configurable thresholds. Defaults: 80°C → 100°C (`Settings::getDefaultMotorTempReductionStart()` / `getDefaultMotorMaxTemp()`, stored in millicelsius).
- **ESC temp:** Linear reduction between two configurable thresholds, defaults per board (`BoardConfig`): Tmotor 80°C → 110°C, XAG 70°C → 80°C.
- All four thresholds are editable from fly-app (`CFG_SET` Thermal) and persist in NVS, so the numbers above are only the factory values. Anything that draws or documents a derating range must read `Settings`, never hardcode these.
- **Throttle engage gate:** motor output stays at `ESC_MIN_PWM` until the filtered throttle clears `throttlePinMin + 2%` of the calibrated range, and releases back below `throttlePinMin + 1%` (hysteresis) — see `ThrottleEngagementLogic`. There is no acceleration ramp; PWM tracks the mapped throttle position directly.
- **XAG wake-up:** 1.5 s at 5% PWM before jumping directly to target when starting from stopped (`XAG_MOTOR_REACTION_DELAY_MS`)
- **Sensor validity & arm-time contract:** motor temp, ESC temp, and battery voltage each have a real validity check (`Temperature`/`BatteryVoltageSensor`'s `isValid()`, `SensorReadingValidity`, host-tested) instead of trusting every reading. Arming opens a per-signal contract (`Power::onArmed()`, `SignalArmContract`, host-tested): valid at arm and later invalid mid-flight disarms (`DisarmReason::MotorTempLost`/`EscTempLost`/`BatteryVoltageLost`); invalid at arm disables that signal's limiting for the whole session, even if it recovers. The arm snapshot is taken by the first `Power::checkSignalLoss()` rather than inside `onArmed()` (so it can't straddle two telemetry samples), and the disarm is debounced by `SIGNAL_LOSS_GRACE_MS` (2 s of continuous invalidity) — see `src/CLAUDE.md` under Power for why both are required. Motor temp has two sources behind one reading (CAN vs NTC), so its contract also snapshots the source tag (`telemetry.getMotorTempOrigin()`) on the same tick as validity: a mid-flight source change is treated as loss of the sensor the pilot armed with and disarms with `DisarmReason::MotorTempSourceChanged` (`MOT SRC`) after the same 2 s `SIGNAL_LOSS_GRACE_MS` debounce, and `shouldLimit()` stops the instant the source diverges so one sensor's thresholds are never applied to the other's reading. No sensor valid at arm (or XAG, whose origin is always `None`) → no source-change disarm.

## Settings (Persistent)

All tunable parameters live in `Settings/` and are stored via `ESP32 Preferences`. Changes survive reboots. Configurable from fly-app over BLE (`CFG_SET`).

## Power alert

The `PowerAlert` component reports which limiters are cutting power via `TELEMETRY.limitCauses` (battery voltage, motor temp, ESC temp). It exposes `getActiveCauses()` only: its 10 s re-fire (beeps) is driven by `PowerAlertLogic`'s `PeriodicTrigger`, and there is no sequence counter for clients. See `PowerAlert/` for the component and `PowerAlertLogic.h` for the host-testable timing logic.

## Flight time

The `sessionSec` field in `TELEMETRY` is a **flight-time counter** (shown by fly-app, reset with `SESSION_RESET`) that survives disarm/re-arm cycles within a power cycle: it counts only while `isArmed && motorRunning`, pauses on disarm or motor stop, resumes from the same value, and is RAM-only (resets on boot). Pure decision logic lives in `src/HourMeter/HourMeterLogic.h` (host-tested in `test/HourMeterLogicTest.cpp`); `src/HourMeter/HourMeter.cpp` is the Arduino wrapper that also owns the persistent `hourMeterSec` total (motor-run seconds, saved to NVS). `SESSION_RESET` zeroes the session counter via a deferred flag — the reset is applied on the next `loop()` tick, never from the BLE callback.

## Wireless Throttle (ESP-NOW)

An optional second ESP32 (the **remote throttle**, firmware in the separate [fly-throttle](https://github.com/rodrigomedeirosbrazil/fly-throttle) repo) reads a Hall sensor + button and sends them to the controller over **ESP-NOW** (channel 1, coexisting with BLE on the C3's single radio; WiFi runs in STA mode only to carry ESP-NOW). The wire contract (`src/RemoteLink/RemoteLinkProtocol.h`) is duplicated byte-for-byte in the fly-throttle repo (its `src/RemoteLinkProtocol.h`) — there is no shared package or submodule. Both copies carry a `REMOTE_LINK_PROTOCOL_VERSION` constant; bump it in both repos whenever the wire format changes, and update `test/RemoteLinkProtocolTest.cpp` in both if the packet layout itself changes size.

- **Source selection:** `Settings::getThrottleSource()` (wired/wireless), set from fly-app (`CFG_SET` System). In wireless mode the controller's `Throttle` `ReadFn` returns the last Hall value received over the link, and the existing AceButton runs on the remote's forwarded **raw button state** (via `SourceSwitchButtonConfig::readButton`) — so calibration and the arming gesture are identical wired/wireless. The remote stays "dumb"; the controller owns all logic.
- **Signal validity & failsafe (`Throttle/ThrottleSignalLogic.h`, host-tested):** a single fault-escalation state machine shared by both wired and wireless throttle sources, parameterized per source. Wireless: no packet for >500 ms → ramp throttle to 0 (stay armed, via the ReadFn feeding 0); >3 s → disarm. Wired: zero tolerance — any invalid reading (see `Throttle/ThrottleWiredValidity.h`) disarms immediately, no ramp-to-zero step. `RemoteLink::isLinkFresh()` supplies the raw "packet arrived recently" signal that feeds the wireless config; `RemoteLink/RemoteLinkLogic.h` no longer exists (absorbed into `ThrottleSignalLogic`). A disarm from either path is latched as a `DisarmReason` (`src/DisarmReason.h`), surfaced via `TELEMETRY.disarmReason` and shown by fly-app as a persistent red chip until the pilot fixes the fault and re-arms.
- **Pairing:** `REMOTE_PAIR` (from fly-app) arms pairing; the next remote heard is saved (MAC in `Settings`). The remote persists the controller MAC in NVS and enters pairing by holding its button while unpaired.
- **Radio:** `RemoteLink::setupRadio()` runs WiFi in STA mode (never associated) on channel 1 and moves the softAP MAC (base + 1) onto STA, because remotes paired while the controller still ran a WiFi AP know it by that address. WiFi stays up for the whole session (ESP-NOW shares the radio and must not be torn down). TX power is pinned to 8.5 dBm — the ESP32-C3 Supermini is unstable at full power (commit f06aa0d). If setting the MAC fails it is logged on serial and the link continues on the base MAC (paired remotes would need re-pairing).
- **Component:** `src/RemoteLink/` (ESP-NOW transport, beep forwarding, pairing; also holds this repo's copy of `RemoteLinkProtocol.h` — see above). Both buzzers stay active — key beeps are forwarded to the remote via `remoteLink.requestBeep()`. The remote's `Armed`/`Stop`/`Disarmed` commands now mirror the controller's own armed+stopped state (`updateSoundState()` in `main.cpp`, using the same `throttle.isEngaged()` hysteresis as `Sound`'s `ArmedIdle` state) — the two used to disagree, with the remote beeping on armed alone.

Remote pinout (ESP32-C3 Supermini): Hall=GPIO0, button=GPIO5, buzzer=GPIO6, red LED=GPIO7 (armed), green LED=GPIO10 (disarmed). Pure decision logic (LED state machine, link-loss, failsafe) lives in host-testable headers tested with `c++ -std=c++17` like `test/PowerTest.cpp`.

## BLE Control Service (fly-app)

The client-facing contract — every UUID, struct offset, opcode and status code,
written for whoever implements the Dart side — is
[docs/BLE-CONTROL-PROTOCOL.md](docs/BLE-CONTROL-PROTOCOL.md). What follows is
the firmware's side of it.

Two GATT services share one BLE server, owned by `BleServerHost` (which also
owns `BLEDevice::init`, the TX power caps and advertising — `Xctod` and
`BleControl` only register services on it).

- **NUS** (`6E400001-…`, advertised) — the `$XCTOD` sentence, **frozen**. It
  has two positional consumers, XCTrack's hand-written `.xcfg` and fly-app's
  `xctod_parser.dart`, with no version handshake, so no field is ever added.
- **Fly Control** (`D4CF0001-9B9D-4BFD-8F7F-40C6989D3EA9`, not advertised) —
  everything the fly-app uses. Not advertised because the 31-byte payload
  cannot hold a second 128-bit UUID; the app discovers it after connecting,
  which makes **service presence the capability handshake** with older
  firmware.

`BleServerHost::onConnect` restarts advertising (gated on the flag
`BluetoothBms` uses to suppress it during a scan). Without that, Bluedroid
stops advertising on the first connection and XCTrack and the app can never
both be connected.

Four characteristics for everything but firmware images — a new feature is a new opcode,
not a new characteristic: `INFO` (read), `TELEMETRY` (notify, a packed 69-byte
struct at 1 Hz), `CMD` (write), `RSP` (notify, responses plus events with
`seq = 0`). `D4CF0006-…` is the DFU image channel (write without response),
a fifth characteristic that exists only because bulk data cannot go through
`CMD`.

**The wire contract is `src/BleControl/ControlProtocol.h`** — pure, host-tested
in `test/ControlProtocolTest.cpp`, and duplicated as Dart in the fly-app repo
with no shared package. Two rules make version skew safe in both directions:
layout is fixed with optional fields as **validity bits rather than absence**,
and changes **append at the end only**, with the reader parsing
`min(received, known)` (`copyKnownPrefix`). `CONTROL_PROTOCOL_VERSION` is
bumped in both repos only when an existing field changes position or meaning.

`CFG_GET`/`CFG_SET` work on four groups (`POWER`, `THERMAL`, `BMS`, `SYSTEM`)
rather than individual keys, so paired values are validated together.
`CFG_SET` seeds from the current settings and overlays only the prefix it
received, so a short write from an older app cannot zero a field it does not
know about. Ranges come from `Settings/SettingsValidation.h`.

Reads are open; writes need the PIN (`AUTH`, per connection, cleared on
disconnect). **While armed, requests are refused by default** with
`ERR_STATE`; the only exceptions are `AUTH`, `CFG_GET`, `BMS_SCAN_STATUS`,
`BMS_SCAN_RESULT` (read-only) and `SESSION_RESET` (a RAM-only counter that
cannot reach the motor). `LOG_*` are not exceptions: they touch flash, so they
are refused while armed. Armed is reported before auth so the app never
prompts for a PIN to do something that would be refused anyway.

`BMS_DETECT` looks like a read and is not: `detectBmsTypeByMac()` disables all
three BMS drivers, pauses advertising and performs a **blocking**
`BLEClient::connect()` to the supplied MAC. Against an unresponsive address
that can outlast the 10 s task watchdog (`WDT_TIMEOUT_S`, `panic=true`) and
reboot the controller — in flight, that cuts the motor. It requires the PIN
and is refused while armed like any other write.

**Flight logs over BLE** (`0x40–0x43`) are offset reads over `CMD`/`RSP`. Naming, paging and retention decisions are pure in `src/Logger/LogListing.h` (host-tested in `test/LogListingTest.cpp`); `src/Logger/LogStore` is the only LittleFS caller. Every reply is bounded by `rspPayloadLimit(negotiatedMtu)` — iOS negotiates 185, so a reply sized to 240 alone would be cut. `LOG_*` answer `ErrBusy` during a DFU transfer (`opRefusedDuringDfu`, applied after the armed/PIN gate so `ErrState` and `ErrAuth` keep precedence). The Logger deletes the oldest log (by name, never the current one) whenever free space drops under 24 KB, at session start and every 30 s while logging, at most 8 deletions per check.

In the telemetry struct, `validity` means **availability** ("does this build
produce this reading at all"); sensor *health* lives in `signalStates`, which
packs the four-state `SignalState` for motor temp, ESC temp and battery
voltage. Those three deliberately have no validity bit —
`Telemetry::isMotorTempValid()` is literally `state == Valid`, so a bit would
be a second answer to the same question, populated from a second call site and
free to drift.

The `CMD` write callback runs on the Bluedroid task and only enqueues
(`ControlRequestQueue`, drop-newest on overflow); `BleControl::handle()` drains
it on the loop task. Nothing touches controller state from the BLE callback.

**Firmware update over BLE** uses `D4CF0006-…` (write without response) for
the image and opcodes `0x50-0x53` for control — `DFU_STATUS` is a plain getter
and joins the read-only exemptions, the other three are ordinary writes and so
are already refused while armed. Transfer decisions live in the host-tested
`BleControl/DfuSession.h`; `DfuSession.cpp` wraps the Arduino `Update`
library.

Two facts about that library drive the design, and both are the opposite of
what the code reads like. `Update.begin()` does **not** erase — it resolves
the OTA partition and nothing else; the erase is lazy inside `_writeBuffer()`,
per 64 KB block as data arrives, so there is no multi-second stall to defer at
begin. What must be deferred is `Update.write()`, which is where that ~150 ms
block erase happens: the BLE callback only feeds a **16 KB FreeRTOS stream
buffer** and `handle()` flushes one 4 KB chunk per tick, same rule as `CMD`.
A plain shared buffer was not enough — with one, `flush()` captured a length,
blocked in `Update.write()`, and then zeroed the cursor, discarding everything
the Bluedroid task had staged during the block while the accepted offset and
the CRC had already counted it. The image transferred, and the commit failed.
A stream buffer is safe for exactly one writer and one reader. Its handle is
detached on abort and freed a loop iteration later, never inside `abort()`
itself: that runs on the loop task while a BLE callback may be mid-stage, so
freeing there trades a data race for a use-after-free. And the CRC is accumulated over
arriving bytes rather than read back from flash, because the updater withholds
the image's first 16 bytes until `end()` so a partial image is never bootable
— a read-back before that point can never match.

See [docs/BLE-CONTROL-PROTOCOL.md](docs/BLE-CONTROL-PROTOCOL.md) for the
client-facing contract.
