# src/ — Fly Controller Firmware Source

This is the main source directory for an ESP32-C3 (LOLIN C3 Mini) Arduino/PlatformIO firmware that controls a drone/UAV motor system. It handles throttle input, ESC PWM output, CAN bus ESC communication, battery monitoring, BMS integration, telemetry, and a WiFi config portal.

## Key Files

| File | Purpose |
|------|---------|
| `main.cpp` | `setup()` / `loop()` entry point; wires everything together |
| `main.h` | Declarations for free functions in main.cpp (`handleEsc`, `checkCanbus`, etc.) |
| `config.h` | All `extern` global object declarations, pin defines, and build constants |
| `config.cpp` | Single translation unit that instantiates all global objects |
| `config_controller.h` | Derives `IS_TMOTOR`, `IS_XAG`, `USES_CAN_BUS` from `CONTROLLER_TYPE` |

## Build Targets

Defined via `-D CONTROLLER_TYPE=N` in `platformio.ini`:

| `CONTROLLER_TYPE` | Env suffix | Macros set | CAN bus |
|---|---|---|---|
| 1 (`XAG`) | `_xag` | `IS_XAG=1` | No |
| 3 (`TMOTOR`) | `_tmotor` (default) | `IS_TMOTOR=1` | Yes (1 Mbps) |

Value `2` was the retired `HOBBYWING` type (removed; recoverable from pre-removal git tags) — the numeric gap is intentional.

`USES_CAN_BUS` is 1 for Tmotor, 0 for XAG. The XAG env also excludes `Tmotor/` and `Canbus/` from the build via `build_src_filter`.

Build command: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor`

Debug builds: add `_debug` suffix (e.g. `lolin_c3_mini_tmotor_debug`), which defines `DEBUG=1` and enables `DEBUG_PRINT` / `DEBUG_PRINTLN` macros from `config.h`.

## Component Directory Pattern

Each feature lives in its own subdirectory with a `.h` and `.cpp`:

```
src/ComponentName/
    ComponentName.h    # Class declaration
    ComponentName.cpp  # Implementation
```

To add a new component:
1. Create `src/ComponentName/ComponentName.h` and `.cpp`
2. Add `extern ComponentName componentName;` to `config.h` (guarded by `#if` if build-specific)
3. Instantiate in `config.cpp`: `ComponentName componentName;`
4. Call `componentName.setup()` in `main.cpp::setup()` (if needed)
5. Call `componentName.handle()` in `main.cpp::loop()` (if needed)

## Global Object Lifecycle

All global singletons follow the same pattern:

- Declared `extern` in `config.h` — this is the single source of truth for what exists
- Defined (instantiated) once in `config.cpp` — no globals in any other `.cpp`
- `setup()` / `init()` called from `main.cpp::setup()` in dependency order
- `handle()` / `update()` called from `main.cpp::loop()`

**Exception**: `ControllerWebServer webServer` and `Logger logger` are defined directly in `main.cpp` (not in `config.cpp`) because they are only used from `main.cpp`.

`Telemetry telemetry` is defined at the bottom of `Telemetry/Telemetry.cpp`.

## Coding Conventions

### Non-blocking loop
Never use `delay()` in `loop()`. All timing uses `millis()`. Components track their own last-run timestamps and return immediately if not enough time has elapsed.

### ReadFn pattern
Components that read from ADC take a function pointer `typedef int (*ReadFn)()` in their constructor. This decouples them from the specific ADC source:

```cpp
// In config.cpp — lambda captures ADS1115 channel
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f   // ADS1115 VREF for GAIN_ONE
);
```

`Throttle`, `Temperature`, and `BatteryVoltageSensor` all use this pattern.

### Units
- Temperatures: millicelsius (`int32_t`) — e.g. 25000 = 25.0°C
- Voltages: millivolts (`uint16_t`) — e.g. 44100 = 44.1V
- Currents: milliamps (`uint32_t`) — e.g. 50000 = 50.0A
- Capacity: milliamp-hours (`uint16_t` / `uint32_t`)

### Conditional compilation
Use `#if IS_TMOTOR`, `#if IS_XAG`, `#if USES_CAN_BUS` — not `#ifdef`. These are always defined as 0 or 1 by `config_controller.h`.

## Component Reference

### ADC — `ADS1115/`
I2C 16-bit ADC (Adafruit ADS1X15). All analog readings go through `ads1115.readChannel(N)`. Channels: 0=Throttle, 1=MotorTemp, 2=EscTemp (XAG), 3=Battery voltage (XAG/Tmotor). Initialized first in `setup()`.

### Throttle — `Throttle/`
Hall sensor input. Accepts a `ReadFn` (value) and `ReadOkFn` (was the read good/fresh). Handles arming state machine, calibration (3-second sweep), and an 8-sample moving average. `throttle.isArmed()` gates ESC output. A separate engage/release hysteresis gate (`ThrottleEngagementLogic`, host-tested in `test/ThrottleEngagementLogicTest.cpp`) protects against Hall-sensor drift/noise at idle — `throttle.isEngaged()` must be true (filtered reading > `throttlePinMin + 2%` of range, released below `+1%`) before `Power` will output anything above `ESC_MIN_PWM`.

Throttle signal validity is split into two host-tested pieces:
- `ThrottleSignalLogic` (`test/ThrottleSignalLogicTest.cpp`) — the fault-escalation state machine shared by wired and wireless sources. Takes a per-tick "is this sample valid" bool plus a `ThrottleSignalConfig{debounceMs, disarmMs, recoveryMs}` and returns `Ok` / `ForceZero` / `Disarm`. Wired uses `{0,0,0}` (any invalid sample disarms immediately); wireless uses `{500,3000,200}` (ramp-to-zero at 500ms, disarm at 3s, plus a 200ms recovery guard against a flapping link).
- `ThrottleWiredValidity` (`test/ThrottleWiredValidityTest.cpp`) — decides whether a *wired* sample itself is valid: the filtered reading must sit inside the calibrated band (asymmetric -20%/+10% margin, clamped inside `[1, adcMaxValue-1]` so it can still catch an open-circuit or short-to-rail reading regardless of calibration span), and the I2C read must not have failed 3+ times in a row (small tolerance for a transient bus glitch).

### Temperature — `Temperature/`
NTC thermistor via Steinhart-Hart (beta=3600, R0=10kΩ). Accepts `ReadFn` + `ReadOkFn` + `adcVoltageRef`. Multiple instances: `motorTemp` (all builds), `escTemp` (XAG only). `isValid()` checks the averaged raw ADC counts against a fixed physical band (91-2954, i.e. -20..150°C) plus I2C read health, via the shared `SensorReadingValidity` (`src/ADS1115/SensorReadingValidity.h`) — also used by `BatteryVoltageSensor`.

### Buzzer — `Buzzer/`
Pure LEDC PWM tone driver: `toneOn(freqHz)`, `toneOff()`, `setVolume(percent)`,
`recalibrate()`. No timing, no patterns, no priority -- that all lives in `Sound/`.
Volume is configurable at runtime and maps 0-100% directly to the 8-bit duty cycle (0 =
silent); the saved volume is applied in `setup()` from `Settings::getBuzzerVolume()`.
Empirical tuning for the current 3.3 V hardware with BC337 transistor stage and passive
piezo buzzer:
- Duty-cycle sweep found the highest perceived volume at about 85% (`217/255`) -- this is
  the default volume; higher duty cycles actually sound quieter on this piezo.
- Frequency sweep found the loudest useful range between `2000 Hz` and `2500 Hz`.

### Sound — `Sound/`
Layered audio policy on top of `Buzzer`. Two layers, resolved every `handle()` tick:
- **Events** (`SoundEvent`, `sound.play(id)`) -- momentary, finite, queued (ring of 4;
  overflow drops the newest so an in-flight sequence is never truncated).
- **State** (`SoundState`, `sound.setState(id)`) -- exactly one persistent sound, declared
  every loop tick from current system state rather than triggered on an edge. Calling
  `setState()` with the same id every tick is a no-op; only a transition restarts the
  cycle. An active event always preempts the state sound immediately; the state resumes
  from the start of its cycle once the queue drains.

`Sound/SoundLogic.h` is the pure decision engine (no Arduino deps, host-tested in
`test/SoundLogicTest.cpp`, same pattern as `PowerAlertLogic.h`/`RemoteLinkLogic.h`).
`SoundPattern.reps == 0` means genuinely continuous -- there is no counter that can
expire mid-flight (a fixed repeat count doing exactly that, at `reps=255`, once caused the
armed alert to silence itself after ~102s). `Sound/PeriodicTrigger.h` is the shared
periodic-fire timing used by both `PowerAlertLogic` and the wireless link-loss warning
(`SoundEvent::LinkLoss`, fired from `main.cpp` while `throttle.isSignalForcedZero()` --
i.e. during the wireless ramp-to-zero window, before the 3 s disarm lands).

`SoundEvent::FaultDisarm` is the alarm for an *unrequested* disarm -- throttle out of
band, link lost, or a power-limiting sensor lost mid-flight. `Throttle::setDisarmed()`
picks it over `SoundEvent::Disarmed` for any non-`Manual` `DisarmReason`. Its finite
`reps=255` (~97 s) is deliberate rather than the bug PR #70 fixed: the motor is already
stopped when it plays, so it exists to get attention on the way down, not to sound
forever. Because it is a queued event, it preempts the state layer on its own -- the
declarative `sound.setState()` in `updateSoundState()` needs no fault-disarm special case.

`sound.getBeepEvents()` returns a ring buffer of up to 8 `BeepEvent` snapshots (oldest
first), each `{seq, frequency, onMs, offMs, reps, layer, active}` -- `layer` is 0 for a
queued event, 1 for the state layer. The web server reads this to include a `buzzer`
array in `/api/telemetry`. The state layer only publishes on a real transition (one entry
starting it, one stopping it), never repeatedly. On the first successful poll the
telemetry page primes `bzLastSeq` to the highest seq (skipping queued-event replay) but
still applies the most recent state event, so the page starts in sync with whatever the
device is already doing. Subsequent polls play fresh queued events immediately and toggle
the state loop on transition, pausing/resuming it around queued events.

### Power — `Power/`
Computes ESC PWM from throttle position, applying battery voltage limiting, motor temp limiting, and ESC temp limiting. `getPwm()` is called every loop to get the current pulse width for `esc.writeMicroseconds()`; output is gated to `ESC_MIN_PWM` unless `throttle.isEngaged()` (see Throttle). No acceleration ramp — PWM tracks the mapped throttle position directly, except on XAG builds where `useSmoothStart` still applies the 1.5 s wake-up delay before jumping to target.
`getActiveLimitCauses()` returns a bitmask (`PowerLimitCause`) of which limiters are currently active. Enum values: `POWER_LIMIT_BATTERY`, `POWER_LIMIT_MOTOR_TEMP`, `POWER_LIMIT_ESC_TEMP`.
Owns the arm-time contract for the three power-limiting signals (motor temp, ESC temp, battery voltage) via `SignalArmContract` (`src/Power/SignalArmContract.h`, host-tested in `test/SignalArmContractTest.cpp`): `onArmed()` (called by `Throttle::setArmed()` on a successful arm) snapshots each signal's current validity. A signal valid at arm that goes invalid mid-flight disarms the system (`throttle.setDisarmed(DisarmReason::MotorTempLost)` etc., via `Power::checkSignalLoss()` — called once per main-loop iteration, never from the async web-server task, since it has side effects that aren't safe to run concurrently with the loop); a signal already invalid at arm has its limiting disabled for the whole session, even if it later reads valid again.

### PowerAlert — `PowerAlert/`
Audible + visual alert when any limiter reduces power below 100%, while armed. Pure decision logic is in `PowerAlertLogic.h` (host-testable, no Arduino deps — see `test/PowerAlertLogicTest.cpp`). The component (`PowerAlert.cpp`) reads `power.getActiveLimitCauses()` + `throttle.isArmed()`, calls `sound.play(SoundEvent::PowerAlert)` + `remoteLink.requestBeep(RemoteBeep::PowerAlert)` on entry and every `POWER_ALERT_BEEP_INTERVAL_MS` (10 s) while limited. `PowerAlertLogic` is a thin wrapper over the shared `PeriodicTrigger` (see `Sound/`). Exposes `getAlertSeq()` (bumped on each fire) and `getActiveCauses()` for the web API. The telemetry page highlights the offending cards in red (persistent while limited) and shows a dismissible alert panel synced to `seq` (reopens on the next 10 s fire after dismissal).

### BatteryMonitor — `BatteryMonitor/`
Coulomb counting SoC. `init()` loads capacity from `Settings`. `update()` integrates current from `telemetry.getBatteryCurrentMilliAmps()`. Auto-recalibrates from voltage when current is near zero for 2 seconds.

### Settings — `Settings/`
Persistent config via ESP32 `Preferences` (NVS). Stores: battery capacity/voltage range, motor/ESC temp limits, WiFi behavior, BMS type and MAC, config PIN, and buzzer volume (key `buzzVol`, 0-100%, default 85%). Initialized first in `setup()`.

### Telemetry — `Telemetry/`
Unified facade over build-specific telemetry sources. `telemetry.getXxx()` delegates via a `TelemetryBackend` struct (function pointers, set at init time). Falls back to `bluetoothBms` data if the primary source returns zero. Consumers should always use `telemetry`, never call `tmotorTelemetry` directly.

- `TmotorTelemetry`: aggregates `batterySensor` + `tmotorCan`; motor temp falls back to ADS1115 when CAN temp is stale
- `XagTelemetry`: aggregates `motorTemp`, `escTemp`, `batterySensor` (all ADS1115)

### CAN Bus — `Canbus/`
Wraps ESP32 TWAI driver. `canbus.receive(&msg)` returns `true` only for frames that should be routed to the ESC handler (returns `false` for internally consumed protocol frames like NodeStatus and GetNodeInfo). `checkCanbus()` in `main.cpp` drains the receive queue each loop and dispatches to `tmotorCan.parseEscMessage()`.

### Tmotor — `Tmotor/`
TM-UAVCAN v2.3 ESC (T-Motor). `TmotorCan`: multi-frame transfer reassembly for ESC_STATUS (1034) and PUSHCAN (1039); handles Status 5 (1154) for motor temp; sends RawCommand (1030) at 400 Hz. `TmotorTelemetry`: snapshot aggregator.

### Xag — `Xag/`
XAG-specific PWM-only build. No CAN bus. `XagTelemetry` reads from ADC sensors only.

### Sensors — `Sensors/`
`BatteryVoltageSensor`: voltage divider via `ReadFn` + `ReadOkFn` + divider ratio + ADS1115 VREF. Used in XAG and Tmotor builds. `isValid()` checks the EMA-smoothed millivolt reading against a fixed plausible range (5000-65000 mV) plus I2C read health, via `SensorReadingValidity`.

### BluetoothBms — `BluetoothBms/`
Facade over the JBD, Daly, and JK BLE BMS backends. Provides pack voltage, current, SoC, cell voltages. Acts as a fallback voltage/current source in `Telemetry`. Also supports web-triggered BLE scanning for BMS device discovery and a live status feed (`GET /api/bms/status`).

### DalyBms / JbdBms / JkBms
Per-vendor BLE BMS protocol implementations behind the `BluetoothBms` facade. All three are instantiated; routing is by `Settings::getBmsType()` at runtime (`BmsTypeJbd`/`BmsTypeDaly`/`BmsTypeJk`). JK uses the JK02 BLE protocol — fixed 300-byte frames, header `55 AA EB 90`, cells at frame offset 6, checksum at the last byte. Frame decoding lives in the host-tested `JkBms/JkBmsParser.h` (`test/JkBmsParserTest.cpp`); the aggregate-field base offset (pack voltage / current / temps / SoC) varies by firmware (118/134/150 seen), so it is auto-located by scanning for the `uint32` matching the cell-voltage sum (pack voltage = sum of series cells) rather than hard-coded. The JK is kicked once and then streams cell-info frames on its own (re-requesting on each poll makes it beep).

### Xctod — `Xctod/`
BLE server that broadcasts telemetry in XCTRACK-compatible format (for paragliding instruments). Sends battery, throttle, motor, ESC, and system status once per second.

### WebServer — `WebServer/`
WiFi AP + captive portal using AsyncWebServer + ElegantOTA. Pages are inline HTML headers in `Pages/`. Handles dashboard, telemetry, config (power, thermal, BMS, system), logs, and OTA firmware updates.

### Button — `Button/`
AceButton wrapper on GPIO5. Short click arms/disarms throttle. Long click (3.5s) triggers calibration. Event callback is `handleButtonEvent()` in `main.cpp`.

### Logger — `Logger/`
LittleFS CSV logger. `startLogging()` is called when the throttle arms; file is closed on disarm. Web UI can download or delete logs.

## Pin Assignments (ESP32-C3)

| Pin | Signal |
|-----|--------|
| GPIO0 | Throttle (legacy ADC, unused when ADS1115 present) |
| GPIO1 | Motor temp (legacy ADC) |
| GPIO2 | CAN TX (SN65HVD230) |
| GPIO3 | CAN RX (SN65HVD230) |
| GPIO5 | Button (pull-up) |
| GPIO6 | Buzzer PWM |
| GPIO7 | ESC PWM |
| GPIO20 | I2C SDA (ADS1115) |
| GPIO21 | I2C SCL (ADS1115) |

ADS1115 channels: A0=Throttle, A1=MotorTemp, A2=EscTemp (XAG), A3=Battery voltage (XAG/Tmotor).
