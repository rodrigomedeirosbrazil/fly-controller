# src/ — Fly Controller Firmware Source

This is the main source directory for an ESP32-C3 (LOLIN C3 Mini) Arduino/PlatformIO firmware that controls a drone/UAV motor system. It handles throttle input, ESC PWM output, CAN bus ESC communication, battery monitoring, BMS integration, telemetry, and a WiFi config portal.

## Key Files

| File | Purpose |
|------|---------|
| `main.cpp` | `setup()` / `loop()` entry point; wires everything together |
| `main.h` | Declarations for free functions in main.cpp (`handleEsc`, `checkCanbus`, etc.) |
| `config.h` | All `extern` global object declarations, pin defines, and build constants |
| `config.cpp` | Single translation unit that instantiates all global objects |
| `config_controller.h` | Derives `IS_HOBBYWING`, `IS_TMOTOR`, `IS_XAG`, `USES_CAN_BUS` from `CONTROLLER_TYPE` |

## Build Targets

Defined via `-D CONTROLLER_TYPE=N` in `platformio.ini`:

| `CONTROLLER_TYPE` | Env suffix | Macros set | CAN bus |
|---|---|---|---|
| 1 (`XAG`) | `_xag` | `IS_XAG=1` | No |
| 2 (`HOBBYWING`) | `_hobbywing` (default) | `IS_HOBBYWING=1` | Yes (500 kbps) |
| 3 (`TMOTOR`) | `_tmotor` | `IS_TMOTOR=1` | Yes (1 Mbps) |

`USES_CAN_BUS` is 1 for Hobbywing and Tmotor, 0 for XAG. The XAG env also excludes `Hobbywing/`, `Tmotor/`, and `Canbus/` from the build via `build_src_filter`.

Build command (run from `controller/`): `~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing`

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
Use `#if IS_HOBBYWING`, `#if IS_TMOTOR`, `#if IS_XAG`, `#if USES_CAN_BUS` — not `#ifdef`. These are always defined as 0 or 1 by `config_controller.h`.

## Component Reference

### ADC — `ADS1115/`
I2C 16-bit ADC (Adafruit ADS1X15). All analog readings go through `ads1115.readChannel(N)`. Channels: 0=Throttle, 1=MotorTemp, 2=EscTemp (XAG), 3=Battery voltage (XAG/Tmotor). Initialized first in `setup()`.

### Throttle — `Throttle/`
Hall sensor input. Accepts `ReadFn`. Handles arming state machine, calibration (3-second sweep), and oversampling (30 samples, 4 readings each). `throttle.isArmed()` gates ESC output.

### Temperature — `Temperature/`
NTC thermistor via Steinhart-Hart (beta=3600, R0=10kΩ). Accepts `ReadFn` + `adcVoltageRef`. Multiple instances: `motorTemp` (all builds), `escTemp` (XAG only).

### Buzzer — `Buzzer/`
Passive buzzer via LEDC PWM. Non-blocking: `setup()` once, then `handle()` every loop tick. Named methods: `beepSystemStart()`, `beepCalibrationStep()`, `beepArmedAlert()`, etc. Supports melodies (sequences of `Note` structs).
Volume is configurable at runtime: `setVolume(percent)` maps 0-100% directly to the 8-bit duty cycle (0 = silent). The saved volume is applied in `setup()` from `Settings::getBuzzerVolume()`, and `beepVolumePreview()` plays a short beep at the current level for live feedback while adjusting (see the web "Sistema" config page).
Empirical tuning for the current 3.3 V hardware with BC337 transistor stage and passive piezo buzzer:
- Duty-cycle sweep found the highest perceived volume at about 85% (`217/255`) — this is the default volume; higher duty cycles actually sound quieter on this piezo.
- Frequency sweep found the loudest useful range between `2000 Hz` and `2500 Hz`.
- Current defaults use `2300 Hz` for general beeps and `2000 Hz` for the armed alert.

`getBeepEvent()` returns a `BeepEvent` snapshot (`seq`, `frequency`, `onMs`, `offMs`, `reps`, `active`) populated by `startBeep()` and cleared by `stop()`. The web server reads this to include a `buzzer` object in `/api/telemetry`, which the telemetry page uses to mirror beeps in the browser via Web Audio API.

### Power — `Power/`
Computes ESC PWM from throttle position, applying battery voltage limiting, motor temp limiting, ESC temp limiting, and ramp rate control. `getPwm()` is called every loop to get the current pulse width for `esc.writeMicroseconds()`.

### BatteryMonitor — `BatteryMonitor/`
Coulomb counting SoC. `init()` loads capacity from `Settings`. `update()` integrates current from `telemetry.getBatteryCurrentMilliAmps()`. Auto-recalibrates from voltage when current is near zero for 2 seconds.

### Settings — `Settings/`
Persistent config via ESP32 `Preferences` (NVS). Stores: battery capacity/voltage range, motor/ESC temp limits, WiFi behavior, BMS type and MAC, config PIN, and buzzer volume (key `buzzVol`, 0-100%, default 85%). Initialized first in `setup()`.

### Telemetry — `Telemetry/`
Unified facade over build-specific telemetry sources. `telemetry.getXxx()` delegates via a `TelemetryBackend` struct (function pointers, set at init time). Falls back to `bluetoothBms` data if the primary source returns zero. Consumers should always use `telemetry`, never call `hobbywingTelemetry` directly.

- `HobbywingTelemetry`: aggregates `hobbywingCan` (CAN) + `motorTemp` (ADS1115)
- `TmotorTelemetry`: aggregates `batterySensor` + `tmotorCan`; motor temp falls back to ADS1115 when CAN temp is stale
- `XagTelemetry`: aggregates `motorTemp`, `escTemp`, `batterySensor` (all ADS1115)

### CAN Bus — `Canbus/`
Wraps ESP32 TWAI driver. `canbus.receive(&msg)` returns `true` only for frames that should be routed to the ESC handler (returns `false` for internally consumed DroneCAN protocol frames like NodeStatus and GetNodeInfo). `checkCanbus()` in `main.cpp` drains the receive queue each loop and dispatches to `hobbywingCan.parseEscMessage()` or `tmotorCan.parseEscMessage()`.

### Hobbywing — `Hobbywing/`
DroneCAN ESC (Hobbywing X-Rotor). `HobbywingCan`: sends throttle, parses telemetry frames. `HobbywingTelemetry`: snapshot aggregator.

### Tmotor — `Tmotor/`
TM-UAVCAN v2.3 ESC (T-Motor). `TmotorCan`: multi-frame transfer reassembly for ESC_STATUS (1034) and PUSHCAN (1039); handles Status 5 (1154) for motor temp; sends RawCommand (1030) at 400 Hz. `TmotorTelemetry`: snapshot aggregator.

### Xag — `Xag/`
XAG-specific PWM-only build. No CAN bus. `XagTelemetry` reads from ADC sensors only.

### Sensors — `Sensors/`
`BatteryVoltageSensor`: voltage divider via `ReadFn` + divider ratio + ADS1115 VREF. Used in XAG and Tmotor builds (not Hobbywing, which gets voltage from CAN).

### BluetoothBms — `BluetoothBms/`
BLE client for JBD or Daly BMS packs. Provides pack voltage, current, SoC, cell voltages. Acts as a fallback voltage/current source in `Telemetry`. Also supports web-triggered BLE scanning for BMS device discovery.

### DalyBms / JbdBms
Serial BMS protocol implementations (wired UART). Objects are instantiated but routing is determined by `Settings::getBmsType()` at runtime.

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
