# Daly BMS BLE Implementation Plan

## Summary

This document defines the implementation plan for adding Daly BMS BLE communication to Fly Controller, in the same functional role currently served by the JBD BMS integration.

The first delivery will support only Daly BLE devices that use the `0xD2` Modbus-like protocol over:

- Service: `FFF0`
- Notify characteristic: `FFF1`
- Write characteristic: `FFF2`

This scope is intentionally limited to the Daly BLE family that is well documented by existing open-source implementations and sample frames. Daly variants that use the `0xA5` protocol are explicitly deferred to a future phase.

The implementation must preserve current behavior for JBD and for builds where no BMS is enabled.

## Goals

- Add a new BLE client backend for Daly BMS.
- Reuse BMS voltage, current, SoC, temperature, and cell data as telemetry fallback when ESC telemetry is unavailable.
- Avoid JBD-specific branching spread across the codebase by introducing a small generic Bluetooth BMS abstraction.
- Keep the configuration flow simple: manual MAC address and explicit BMS type selection.
- Preserve safe behavior already present in JBD integration, especially avoiding BLE connection attempts while throttle is armed.

## Explicit Non-Goals for v1

- No support for Daly `0xA5` BLE protocol families.
- No BLE scanning or auto-discovery.
- No BMS write commands, parameter changes, or MOS/FET control.
- No Daly cloud/Wi-Fi recovery workflow in firmware.
- No attempt to unify JBD and Daly protocols internally beyond the minimum shared application-facing abstraction.

## Research Baseline

The implementation must be based on the following references:

- `syssi/esphome-daly-bms`
- `patman15/aiobmsble`
- `fl4p/batmon-ha`

Key protocol findings confirmed by those repositories:

- Daly `0xD2` BLE devices commonly expose `FFF0/FFF1/FFF2`.
- The main read request is an 8-byte frame built like Modbus RTU:

```text
D2 03 00 00 00 3E D7 B9
```

- Request format:

```text
[addr=0xD2] [fn] [reg_hi] [reg_lo] [value_hi] [value_lo] [crc_lo] [crc_hi]
```

- Responses are CRC16 Modbus protected.
- The most useful telemetry payload is the status read starting at register `0x0000`.
- The common status lengths are 62 registers and 80 registers. v1 must require only 62-register compatibility, but the parser may tolerate 80-register responses if they arrive.
- Current is encoded with a `30000` offset and `0.1 A` resolution.
- Cell voltages and temperature values are directly embedded in the status response.

Compatibility finding:

- Daly BLE support is fragmented by protocol family.
- The repository evidence is strongest for the `0xD2` family used by H/K/M/S style BLE modules and K-Series devices.
- The `0xA5` family must be handled later as a separate implementation effort.

## Current Project Impact

The existing JBD integration is currently coupled into:

- `src/main.cpp`
- `src/config.h`
- `src/config.cpp`
- `src/Telemetry/Telemetry.cpp`
- `src/Telemetry/TelemetryAvailability.cpp`
- `src/Settings/Settings.h`
- `src/Settings/Settings.cpp`
- `src/WebServer/ControllerWebServer.cpp`
- `src/WebServer/Pages/ConfigPage.h`

Current behavior to preserve:

- BLE client uses a fixed MAC address configured by the web UI.
- No scan is performed.
- Connection attempts are skipped while throttle is armed.
- Telemetry falls back to JBD BMS voltage/current when ESC telemetry returns zero or is unavailable.
- The web UI exposes a `bms` block with `available`, `cellMinMv`, `cellMaxMv`, and `cellDeltaMv`.

## Target Architecture

### 1. Introduce an application-facing Bluetooth BMS abstraction

Add a lightweight abstraction layer that represents the currently active Bluetooth BMS backend. It does not need to be a complex inheritance hierarchy. A small facade with backend selection is sufficient as long as application code no longer references `jbdBms` directly.

Required abstraction surface:

- `init()`
- `update()`
- `isConnected()`
- `hasData()`
- `hasCellData()`
- `getPackVoltageMilliVolts()`
- `getPackCurrentMilliAmps()`
- `getSoCPercent()`
- `getCellCount()`
- `getCellVoltageMilliVolts(index)`
- `getCellMinMilliVolts()`
- `getCellMaxMilliVolts()`
- `getCellDeltaMilliVolts()`
- `getTempCount()`
- `getTempCelsius(index)`

This abstraction must select one backend at runtime based on settings:

- `none`
- `jbd`
- `daly`

### 2. Keep `JbdBms` as an existing backend

`JbdBms` should remain functionally unchanged except for any small adaptation needed to satisfy the new abstraction surface.

No protocol changes should be made to JBD in this task.

### 3. Add `DalyBms` as a new backend

Create a new component in `src/DalyBms/` that mirrors the operational model used by `JbdBms`:

- BLE client created once during initialization
- state machine driven by `update()`
- direct MAC connection
- explicit characteristic lookup
- subscribe to notify characteristic
- periodic request transmission
- internal parser and cached decoded values
- reconnect logic on failure

### 4. Make application code depend on the generic BMS facade

The following behaviors must switch to the generic active BMS source:

- telemetry fallback in `Telemetry`
- BMS availability checks in `TelemetryAvailability`
- BMS block in telemetry web JSON
- initialization and periodic update in `main.cpp`

## DalyBms Protocol and Behavior

### BLE connection rules

- Use the configured MAC address only.
- Do not scan.
- Do not attempt a connection while throttle is armed.
- Retry connection at a fixed interval similar to current JBD behavior.
- Treat missing service or characteristics as recoverable failure and reset to idle for the next retry.

### GATT layout

- Service UUID: `0000fff0-0000-1000-8000-00805f9b34fb`
- RX/notify UUID: `0000fff1-0000-1000-8000-00805f9b34fb`
- TX/write UUID: `0000fff2-0000-1000-8000-00805f9b34fb`

### Request strategy

v1 request strategy:

- Send only the status read command for 62 registers:

```text
D2 03 00 00 00 3E D7 B9
```

- Poll on a fixed interval close to the current JBD cadence.
- A dedicated TX queue/task is optional. It should be used only if BLE writes prove blocking enough to justify it. If the existing JBD pattern is reused, that is acceptable.

### Response validation

The parser must reject any response that fails one of these checks:

- frame shorter than minimum Modbus response length
- first byte not `0xD2`
- function byte not expected for read response
- inconsistent payload length
- CRC16 Modbus mismatch

On invalid frames:

- keep the last known good decoded values
- do not mark new data as valid
- do not crash or permanently poison the connection state

### Fields to decode in v1

The implementation must decode and cache at least:

- total pack voltage in mV
- pack current in mA
- SoC percent
- remaining capacity in mAh-equivalent units if present
- cycle count
- cell count
- temperature sensor count
- per-cell voltage array
- per-temperature array
- charge/discharge/balance status bits
- delta cell voltage

If an 80-register response is received instead of 62:

- parse the shared fields exactly the same
- ignore extra fields unless trivial to expose
- do not require 80-register-only values for any application behavior

### Value conversions

Use these conversion defaults:

- total voltage: raw value `* 100` to mV if source is `0.1 V`
- current: `(raw - 30000) * 100` to mA
- SoC: use integer percent if available from `0.1%` source by dividing appropriately
- cell voltage: convert to mV
- temperature: raw value minus `40` to Celsius where that encoding is used by the protocol

The code should store internal cached values in the same units already used by the project:

- voltage in mV
- current in mA
- temperatures in Celsius or milliCelsius only when the consuming API requires it

## Settings and UI Changes

### Replace JBD-only settings with generic Bluetooth BMS settings

Add these runtime settings:

- `bmsType`
- `bmsMac`

`bmsType` values:

- `none`
- `jbd`
- `daly`

### Backward compatibility strategy

During `Settings::load()`:

- if the new generic BMS keys exist, use them
- otherwise migrate legacy JBD keys into the new model:
  - if `jbdBmsEnabled == true` and `jbdBmsMac` is non-empty, set `bmsType = jbd` and `bmsMac = jbdBmsMac`
  - otherwise set `bmsType = none`

During `Settings::save()`:

- persist only the new generic keys
- legacy keys may remain untouched in Preferences, but the firmware must stop depending on them

### Web configuration page

Replace the current “JBD BMS” section with a generic “Bluetooth BMS” section containing:

- checkbox or select for enabling the feature
- explicit type selector: `JBD` or `Daly`
- MAC address field

Behavior rules:

- if disabled, `bmsType = none`
- if enabled, both type and valid MAC must be supplied
- MAC validation remains `XX:XX:XX:XX:XX:XX`

Copy should clearly state:

- manual MAC entry
- no device scan
- Daly support in this firmware targets the BLE `0xD2` family only

## Telemetry and Web API Behavior

### Telemetry fallback behavior

The current fallback logic must remain conceptually the same:

- primary source remains the board-specific ESC/backend telemetry
- if battery voltage is zero and active Bluetooth BMS has data, use BMS pack voltage
- if battery current is zero and active Bluetooth BMS has data, use absolute BMS current

This fallback must work for both JBD and Daly without frontend changes.

### BMS availability helpers

`isBmsDataAvailable()` and `isBmsCellDataAvailable()` must become backend-agnostic.

### Telemetry JSON

The JSON contract must remain compatible for the frontend:

```json
{
  "availability": {
    "bms": true,
    "bmsCells": true
  },
  "bms": {
    "available": true,
    "tempMaxC": 32,
    "cellMinMv": 3412,
    "cellMaxMv": 3458,
    "cellDeltaMv": 46
  }
}
```

The implementation must not expose separate `jbd` and `daly` blocks. The frontend should see one generic BMS shape.

## File-Level Implementation Plan

### Phase 1: generic Bluetooth BMS facade

- Add a new facade component that owns backend selection and exposes the generic BMS API.
- Register the new facade in `config.h` and `config.cpp`.
- Update `main.cpp` to initialize and update the facade instead of calling `jbdBms` directly.

### Phase 2: adapt JBD to the facade

- Bridge existing `JbdBms` data into the new generic BMS surface.
- Preserve all existing JBD behavior and data interpretation.

### Phase 3: implement `DalyBms`

- Add `src/DalyBms/DalyBms.h`
- Add `src/DalyBms/DalyBms.cpp`
- Implement BLE state machine, notify callback, command building, CRC validation, and status frame decode.
- Cache parsed values in project-native units.

### Phase 4: settings and config UI migration

- Replace JBD-only settings accessors with generic BMS accessors.
- Add migration logic from the old JBD settings.
- Update `/config/values`, `/config/save`, and the config page UI.

### Phase 5: telemetry and web integration

- Switch `Telemetry` and `TelemetryAvailability` to the generic BMS source.
- Update web telemetry JSON to read the active BMS source.

### Phase 6: validation and cleanup

- Verify JBD behavior did not regress.
- Verify Daly values decode correctly from known sample frames.
- Ensure all builds still compile after the abstraction changes.

## Test Plan

### Static and parser validation

- Verify CRC16 Modbus implementation against the known command `D2 03 00 00 00 3E D7 B9`.
- Validate parser against at least one known-good sample response from open-source tests.
- Validate rejection of bad CRC frame.
- Validate rejection of too-short frame.
- Validate rejection of wrong start byte or wrong function.

### Backend behavior tests

- JBD selected: generic BMS facade returns JBD values exactly as before.
- Daly selected: facade returns Daly cached values after a valid frame.
- No BMS selected: facade reports no data and never attempts BLE connection.
- Armed throttle: both JBD and Daly connection attempts remain suppressed.
- Disconnect after successful connection: backend returns to retry flow without hanging.

### Settings and migration tests

- Fresh install defaults to `bmsType = none`.
- Legacy Preferences with enabled JBD migrate to `bmsType = jbd`.
- Legacy Preferences with disabled JBD migrate to `bmsType = none`.
- Saving new generic settings does not require legacy keys.

### Web/API validation

- `/config/values` returns generic BMS settings.
- `/config/save` accepts and validates generic BMS settings.
- `/api/telemetry` keeps the existing `bms` JSON contract.
- Frontend still renders BMS values without JBD-specific assumptions.

### Build validation

Run or verify compilation for:

- `lolin_c3_mini_hobbywing`
- `lolin_c3_mini_tmotor`
- `lolin_c3_mini_xag`

If full PlatformIO builds are not run in automation, at minimum ensure the implementation remains isolated from controller-specific code paths and does not break conditional compilation.

## Delivery Sequence

Recommended commit sequence during implementation:

1. `refactor(bms): add generic bluetooth bms facade`
2. `feat(bms): add daly bms ble backend`
3. `refactor(settings): replace jbd-only bms config with generic bms settings`
4. `refactor(telemetry): use active bms backend for fallback and availability`
5. `feat(web): expose generic bluetooth bms configuration and telemetry`
6. `test(bms): validate daly parser and jbd compatibility`

## Assumptions and Defaults

- v1 supports only Daly BLE `0xD2` devices.
- Manual MAC entry is the only supported device selection workflow.
- The BLE runtime initialized by XCTOD remains compatible with adding another BLE client backend.
- The project should preserve current integer telemetry transport units and frontend JSON shape.
- Extra Daly features available in 80-register or settings/version/password responses are not required for the first delivery.
- If a specific Daly hardware unit later proves to use `0xA5`, that will be handled as a separate follow-up feature, not as a bug fix to this v1 scope.

## Future Phase: Daly `0xA5`

After `0xD2` support is stable on hardware, a future document or follow-up task can extend support for Daly BLE families that use the `0xA5` protocol. That work should be treated as a separate backend or protocol mode because:

- framing is different
- parser and checksum rules differ
- repository evidence is weaker and less uniform
- mixing both families in the first delivery would increase implementation and test risk significantly
