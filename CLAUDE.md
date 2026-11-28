# Fly Controller — Claude Code Guide

ESP32-C3 Arduino/PlatformIO firmware for intelligent drone/UAV flight control. Supports three ESC types via three build targets.

## Build System

**Platform:** PlatformIO · **Board:** `lolin_c3_mini` (ESP32-C3) · **Framework:** Arduino

```bash
# Build
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing

# Upload
~/.platformio/penv/bin/pio run -e <env> -t upload

# Serial monitor
~/.platformio/penv/bin/pio device monitor -e <env>
```

Do NOT run `pio` without the full path — it may not be in PATH.

## Releases

Pushing a git tag triggers `.github/workflows/build-and-release.yml`, which builds all three controller targets and publishes a GitHub Release with the firmware binaries. Tags follow the `YYYY-MM-DD.N` convention (e.g. `2026-05-29.1`, where `N` increments for multiple releases on the same day). The tag becomes `APP_VERSION` (via generated `src/Version.h`) and the release-notes changelog spans from the previous tag (resolved via `git describe` on the tagged commit's parent) to the new tag.

## Build Targets

| Environment | `CONTROLLER_TYPE` | Macro | CAN Bus | Protocol |
|---|---|---|---|---|
| `lolin_c3_mini_tmotor` (default) | 3 | `IS_TMOTOR` | 1 Mbps | UAVCAN |
| `lolin_c3_mini_xag` | 1 | `IS_XAG` | None | PWM-only |
| `lolin_c3_mini_hobbywing` | 2 | `IS_HOBBYWING` | 500 kbps | PWM throttle, DroneCAN telemetry (receive-only) |

`config_controller.h` derives `IS_TMOTOR` / `IS_XAG` / `IS_HOBBYWING` / `USES_CAN_BUS` from `CONTROLLER_TYPE`. Always use `#if IS_TMOTOR` (not `#ifdef`).

## Project Structure

```
src/
├── main.cpp / main.h         # setup() + loop(); routes CAN frames, calls all components
├── config.h / config.cpp     # All extern declarations + global object instantiations
├── config_controller.h       # Build-type macros derived from CONTROLLER_TYPE
├── ADS1115/                  # I2C ADC (Tmotor: throttle + motor temp)
├── BatteryMonitor/           # Coulomb counting + SoC from voltage
├── BluetoothBms/             # BLE BMS integration
├── BmsAlert/                 # BMS alerts (low cell, high delta, high temp): BmsAlertLogic (pure) + BmsAlert component
├── Button/                   # Gesture logic: click + hold (arm), hold (disarm ramp)
├── Buzzer/                   # LEDC PWM tone driver (hardware only)
├── Canbus/                   # TWAI receive() — returns raw frames to main.cpp
├── DalyBms/                  # Daly BMS (BLE) protocol
├── Hobbywing/                # HobbywingCan (receive-only) + HobbywingTelemetry + pure HobbywingCanParser
├── JbdBms/                   # JBD BMS (BLE) protocol
├── AntBms/                   # ANT/JNFN BMS (BLE, `7E A1` protocol) + frame parser
├── JkBms/                    # JK BMS (BLE, JK02) protocol + frame parser
├── Logger/                   # CSV logging to LittleFS + LogListing (pure) / LogStore (LittleFS) for BLE log access
├── Power/                    # Available-power calculation + limiter ceilings
├── Sensors/                  # BatteryVoltageSensor (XAG voltage divider)
├── Settings/                 # Persistent config via ESP32 Preferences
├── Sound/                    # Layered audio policy: event queue + persistent state
├── Telemetry/                # Facade (delegates to *Telemetry) + TelemetryData struct
├── Temperature/              # NTC thermistor via ReadFn
├── Throttle/                 # Hall sensor via ReadFn + calibration + engage gate
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
`telemetry.getXxx()` delegates to `TmotorTelemetry` / `XagTelemetry` / `HobbywingTelemetry` based on build target. Always use the facade — never access ESC objects directly for telemetry.

### CAN Bus Routing
`Canbus::receive()` is non-blocking and returns raw frames. `main.cpp` routes them:
```cpp
while (canbus.receive(&msg)) {
    tmotorCan.parseEscMessage(msg);   // Tmotor build
    // hobbywingCan.parseEscMessage(msg) on the Hobbywing build
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
| 2 | CAN TX (TWAI) | Tmotor, Hobbywing |
| 3 | CAN RX (TWAI) / Battery voltage | Tmotor, Hobbywing: CAN RX; XAG: voltage divider |
| 4 | ESC temperature (NTC) | XAG only |
| 5 | Button | AceButton, interrupt |
| 6 | Buzzer | PWM, passive piezo |
| 7 | ESC PWM output | 1050–1950 μs |

## Power & Safety Logic

- **Battery voltage:** Progressive reduction below nominal (configurable via Settings)
- **Motor temp:** Linear reduction between two configurable thresholds. Defaults: 80°C → 100°C (`Settings::getDefaultMotorTempReductionStart()` / `getDefaultMotorMaxTemp()`, stored in millicelsius).
- **T-Motor CAN motor temp:** EMA-filtered (tau 5 s) before derating, display and log; an out-of-range sample is dropped, and the reading goes stale after 1 s without a plausible one.
- **ESC temp:** Linear reduction between two configurable thresholds, defaults per board (`BoardConfig`): Tmotor 80°C → 110°C, Hobbywing 80°C → 110°C, XAG 70°C → 80°C.
- All four thresholds are editable from fly-app (`CFG_SET` Thermal) and persist in NVS, so the numbers above are only the factory values. Anything that draws or documents a derating range must read `Settings`, never hardcode these.
- **Throttle engage gate:** motor output stays at `ESC_MIN_PWM` until the filtered throttle clears `throttlePinMin + 2%` of the calibrated range, and releases back below `throttlePinMin + 1%` (hysteresis) — see `ThrottleEngagementLogic`. There is no acceleration ramp; PWM tracks the mapped throttle position directly.
- **XAG wake-up:** 1.5 s at 5% PWM before jumping directly to target when starting from stopped (`XAG_MOTOR_REACTION_DELAY_MS`)
- **Sensor validity:** motor temp, ESC temp, and battery voltage each have a real validity check (`Temperature`/`BatteryVoltageSensor`'s `isValid()`, `SensorReadingValidity`, host-tested) instead of trusting every reading. The T-Motor CAN motor-temperature ceiling is 160 °C, 10 °C above the settable maximum, enforced by a `static_assert`; the ESC ceiling (120 °C) is still below the settable ESC maximum (known gap, not addressed).
- **Temperature sensors never cut the motor:** motor/ESC temp derating applies only while the *configured* source reads valid — for motor temp on Tmotor that is `Settings::getMotorTempSource()` (CAN, or NTC when forced); the CAN→NTC fallback is shown to the pilot but never used for derating. When it stops, derating stops on that tick and `TempProtectionMonitor` (host-tested) flips the protection to *suspended* after 2 s (`TEMP_PROTECTION_DEBOUNCE_MS`, both edges). `TempProtectionAlert` (host-tested `TempProtectionAlertLogic`) then alarms while armed: a falling two-note motif at 0, 10, 30, 60 s and every 60 s after, and a rising motif once when every protection is back. Suspended state is in `TELEMETRY.flags` (`MotorTempProtectionOff`/`EscTempProtectionOff`). `MOT ERR`/`ESC ERR`/`MOT SRC` are retired and never emitted.
- **Wired throttle failsafe:** zero tolerance — any invalid reading (see `Throttle/ThrottleWiredValidity.h`) disarms immediately through `ThrottleSignalLogic` (host-tested; generic, kept for the day a second source returns). The disarm is latched as a `DisarmReason` (`src/DisarmReason.h`), surfaced via `TELEMETRY.disarmReason` and shown by fly-app as a persistent red chip until the pilot fixes the fault and re-arms.
- **Battery arm-time contract:** arming opens `SignalArmContract` (host-tested) for battery voltage only: valid at arm and later invalid mid-flight disarms (`DisarmReason::BatteryVoltageLost`); invalid at arm disables battery limiting for the whole session. The snapshot is taken by the first `Power::checkSignalLoss()` rather than inside `onArmed()` (so it can't straddle two telemetry samples), and the disarm is debounced by `SIGNAL_LOSS_GRACE_MS` (2 s) — see `src/CLAUDE.md` under Power.

## Settings (Persistent)

All tunable parameters live in `Settings/` and are stored via `ESP32 Preferences`. Changes survive reboots. Configurable from fly-app over BLE (`CFG_SET`).

## BMS alerts

`BmsAlert` (`src/BmsAlert/`) raises three informational alerts from BMS data: **low cell** (min cell at or under the per-cell cutoff *and* cell delta above a gate), **high delta** (delta above a maximum) and **high temperature** (BMS max probe temperature at or above a limit). The per-cell cutoff is `Settings::getBatteryMinVoltage()` divided by the cell count the BMS reports (`BluetoothBms::getCellCount()`, never `BATTERY_CELL_COUNT`); cell count 0 disables low cell. Defaults: delta gate 100 mV, max delta 200 mV, temp limit 55 C (ranges 20-500 mV, 50-1000 mV, 30-80 C, persisted in `Settings`). Hysteresis: low cell exits at cell >= cutoff + 50 mV or delta < gate - 10 mV; delta exits 10 mV under the max; temperature exits 3 C under the limit. Every edge is debounced 3 s (`BMS_ALERT_DEBOUNCE_MS`), except that unavailable BMS data clears the alerts at once. Availability alone is enough for that: every BMS driver clears its readings when the link drops (see `src/CLAUDE.md`, BluetoothBms).

Sound plays only while armed, reusing `TempProtectionAlertLogic`'s cadence (0, 10, 30, 60 s, then every 60 s, and a rising "all clear" motif when the mask returns to zero) with its own motif (`SoundEvent::BmsAlert`, three long 2200 Hz pulses) The bitmask is exposed by `getActiveAlerts()` regardless of armed state. **These alerts never reach `Power`: they never limit the motor and never disarm.** The decision logic is `src/BmsAlert/BmsAlertLogic.h`, host-tested in `test/BmsAlertLogicTest.cpp`.

## Power alert

The `PowerAlert` component reports which limiters are cutting power via `TELEMETRY.limitCauses` (battery voltage, motor temp, ESC temp). It exposes `getActiveCauses()` only: its 10 s re-fire (beeps) is driven by `PowerAlertLogic`'s `PeriodicTrigger`, and there is no sequence counter for clients. See `PowerAlert/` for the component and `PowerAlertLogic.h` for the host-testable timing logic.

## Flight time

The `sessionSec` field in `TELEMETRY` is a **flight-time counter** (shown by fly-app, reset with `SESSION_RESET`) that survives disarm/re-arm cycles within a power cycle: it counts only while `isArmed && motorRunning`, pauses on disarm or motor stop, resumes from the same value, and is RAM-only (resets on boot). Pure decision logic lives in `src/HourMeter/HourMeterLogic.h` (host-tested in `test/HourMeterLogicTest.cpp`); `src/HourMeter/HourMeter.cpp` is the Arduino wrapper that also owns the persistent `hourMeterSec` total (motor-run seconds, saved to NVS). `SESSION_RESET` zeroes the session counter via a deferred flag — the reset is applied on the next `loop()` tick, never from the BLE callback.

## Remaining flight time

`RemainingTime` (`src/RemainingTime/`) estimates **motor-on** minutes left at the regime the pilot is flying now: usable energy ÷ an EMA of current over 10 s blocks of motor-on time (tau 120 s when current rises, 600 s when it falls), frozen off-motor, seeded from the persisted `referenceCurrentA` (factory 90 A Tmotor / 90 A Hobbywing / 75 A XAG) and nudged by each qualifying flight (≥ 180 s measured motor time: `(ref + flightAvg) / 2`; a flight averaging under 10 A is ignored, since that average means a sensor reading zero). Without a current reading it falls back to the pack voltage (anchored at rest, dead-reckoned while running) and reports quality `Voltage`. Published every 10 s in whole minutes to BLE `TELEMETRY`, `$XCTOD` field 17 and the CSV log. Decision logic is host-tested in `test/RemainingTimeLogicTest.cpp`.

## BMS capacity sync

`BmsCapacitySync` (`src/BmsCapacitySync/`) makes the BMS-reported nominal
capacity the capacity the coulomb count uses — **always**, no toggle, and
**runtime only**: `Settings::batteryCapacityMah` and NVS are never touched,
since the BMS reports the value again on every connection. The configured
value stays saved and is used whenever no BMS reports one (no BMS, Daly, or
before the BMS connects after boot); once adopted, the BMS value stays in
use until reboot even if the BMS link drops, since the pack has not changed. JBD (register 0x03), JK (cell-info
frame, +28 from the pack-voltage field, read only when the pack-voltage scan
matched the cell sum) and ANT (status frame, in µAh) report it; Daly does not. Applied once the value has
been stable 3 s (`BMS_CAPACITY_STABLE_MS`), is inside the `CFG_SET` range
(`validateCapacity`, 1000–65000 mAh; outside it is logged once and ignored),
differs from the BMS value already adopted and the controller is disarmed (pending while
armed). `BatteryMonitor` keeps both values: `setBmsCapacity()` rescales the
count **keeping the percentage**; `setCapacity()` (`CFG_SET`) stores the
configured value and resets to full only when that value is the one in use.
The BMS value is in `TELEMETRY` (`bmsDesignCapacityMah`, validity
`BmsCapacity`); `CFG_GET POWER` returns the configured value. Decision logic is
host-tested in `test/BmsCapacitySyncLogicTest.cpp`.

## Wireless remote (removed)

The ESP-NOW remote throttle and WiFi were removed on 2026-11-17 so BLE has the single 2.4 GHz radio to itself. To bring them back, restore from git history: commit `2a70920`, the last one before the removal (PR #91, squashed as `5d53652`) (`RemoteLink/`, its protocol header and tests, the wireless branches of `Throttle`/`Button`, `Settings` `thrSrc`/`rmtMac`). The firmware is wired-only now; nothing reads the stale `thrSrc`/`rmtMac` NVS keys, so a unit that had wireless saved boots wired.

The fly-app contract was **soft-removed**: fields and opcodes are retired, not deleted, so existing app builds keep working. `Capability::RemoteLink` (`0x0008`) is never set; `REMOTE_PAIR`/`REMOTE_FORGET` (`0x24`/`0x25`) answer `ErrBadOp`; `CFG_GET SYSTEM` returns `throttleSource = 0` and a zero `remoteMac`, and `CFG_SET SYSTEM` answers `ErrBadArg` for any `throttleSource != 0`; `DisarmReason::ThrottleLinkLost` (`LINK ERR`) is reserved and never produced. `CONTROL_PROTOCOL_VERSION` was not bumped. The remote's firmware lives in the separate [fly-throttle](https://github.com/rodrigomedeirosbrazil/fly-throttle) repo, which is unchanged.

## BLE Control Service (fly-app)

The client-facing contract — every UUID, struct offset, opcode and status code,
written for whoever implements the Dart side — is
[docs/BLE-CONTROL-PROTOCOL.md](docs/BLE-CONTROL-PROTOCOL.md). What follows is
the firmware's side of it.

Two GATT services share one BLE server, owned by `BleServerHost` (which also
owns `BLEDevice::init`, the TX power caps and advertising — `Xctod` and
`BleControl` only register services on it).

- **NUS** (`6E400001-…`, advertised) — the `$XCTOD` sentence, **append-only**.
  It has two positional consumers, XCTrack's hand-written `.xcfg` and fly-app's
  `xctod_parser.dart`, with no version handshake, so fields 1–16 never move or
  change meaning; new fields go at the end only (field 17 is `remaining_min`),
  and the fly-app accepts trailing fields since its release `2026-10-12.1`.
  Anything richer still belongs in Fly Control.
- **Fly Control** (`D4CF0001-9B9D-4BFD-8F7F-40C6989D3EA9`, not advertised) —
  everything the fly-app uses. Not advertised because the 31-byte payload
  cannot hold a second 128-bit UUID; the app discovers it after connecting,
  which makes **service presence the capability handshake** with older
  firmware.

`BleServerHost::onConnect` restarts advertising (gated on the flag
`BluetoothBms` uses to suppress it during a scan). Without that, Bluedroid
stops advertising on the first connection and XCTrack and the app can never
both be connected.

**Link policy** (pure decisions in `src/BleServerHost/BleLinkPolicy.h`, host-tested in
`test/BleLinkPolicyTest.cpp`; `BleServerHost` wires them to Bluedroid). A body between
the phone and the controller fades the link for a second or two, and iOS defaults to a
~720 ms supervision timeout, so on every peripheral-role connect
(`link_role == 1`) the controller asks the phone for 30-45 ms interval, latency 0 and a
6 s timeout (`esp_ble_gap_update_conn_params`; inside Apple's accessory rules). If the
phone later applies a timeout under 6 s the GAP handler re-requests **once** per link,
never more, so there is no tug-of-war. The BMS client link is opened with its own
preferred params (60-100 ms, latency 0, 6 s; `esp_ble_gap_set_prefer_conn_params` before
each `connect()` in the JBD/Daly/JK/ANT drivers), since it is polled every 2 s.
Advertising runs at 20-40 ms with no phone connected and 250-400 ms once one is, which
cuts its airtime from roughly 10% to 1% while still letting a second central find us.
Links are counted by **peripheral role only**: BMS client links also fire the server's
`onConnect`/`onDisconnect`, so `getConnectedCount()` is never used for this. That fixed a
bug where, with a BMS connected, the "last phone left" MTU reset never ran and a larger
MTU carried over to the next phone. All start/stop advertising calls stay on the loop task.

Four characteristics for everything but firmware images — a new feature is a new opcode,
not a new characteristic: `INFO` (read), `TELEMETRY` (notify, a packed 79-byte
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
four BMS drivers, pauses advertising and performs a **blocking**
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
