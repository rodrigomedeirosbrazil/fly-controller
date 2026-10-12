# Xctod BLE Telemetry Protocol

`Xctod` (`src/Xctod/`) broadcasts a proprietary, comma-separated telemetry
sentence over BLE, intended to be read by XCTrack (or any app/instrument
supporting a custom BLE telemetry source) via a fixed-position custom
sentence. It is **not** a standard industry protocol (not LK8000/LXNAV NMEA,
LiveTrack24, or FlyMaster) — the format is defined entirely by field order
in `Xctod::write()`.

The CSV flight log (`src/TelemetryLogger/`, written to LittleFS while
armed) carries the same telemetry fields in the same order, so the two are
kept field-for-field aligned — see [Differences from the CSV log](#differences-from-the-csv-log)
below for the few places they diverge.

## BLE service

| | |
|---|---|
| Service UUID | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| Characteristic UUID (TX, NOTIFY) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |
| Update rate | 1 Hz |
| Sentence format | `$XCTOD,<field1>,<field2>,...,<fieldN>\r\n` |
| Buffer size | 256 bytes |

## Field table

Fields are 1-indexed after the `$XCTOD,` prefix (field 1 is the first
value after the prefix). Apps that read by position (e.g. XCTrack's
`WExternalData` widget `index` setting) must use this numbering.

| # | Field | Unit / format | Source | Blank when |
|---|-------|----------------|--------|------------|
| 1 | `battery_percent_cc` | integer % | `batteryMonitor.getSoC()` | never |
| 2 | `battery_percent_voltage` | integer % | `batteryMonitor.getSoCFromVoltage()` | never |
| 3 | `voltage` | `V.mmm` | `telemetry.getBatteryVoltageMilliVolts()` | battery voltage signal not `Valid` |
| 4 | `power_kw` | `kW.d` | computed from voltage × current | voltage invalid, or current unavailable |
| 5 | `throttle_percent` | integer % | `throttle.getThrottlePercentage()` | never |
| 6 | `throttle_raw` | integer | `throttle.getThrottleRaw()` | never |
| 7 | `power_percent` | integer % | `power.getPower()` | never |
| 8 | `motor_temp` | integer °C | `telemetry.getMotorTempMilliCelsius()` | motor temp signal not `Valid` |
| 9 | `motor_temp_src` | `can` / `ntc` | `telemetry.getMotorTempOrigin()` | XAG builds, or before the first Tmotor reading |
| 10 | `rpm` | integer | `telemetry.getRpm()` | current/RPM not available (see `isCurrentAvailable()`) |
| 11 | `esc_current` | integer A | `telemetry.getBatteryCurrentMilliAmps()` | current/RPM not available |
| 12 | `esc_temp` | integer °C | `telemetry.getEscTempMilliCelsius()` | ESC temp signal not `Valid` |
| 13 | `system_status` | `ARMED` / `DISARMED` / fault code | `throttle.isArmed()` + `throttle.getDisarmReason()` | never (always one of the three) |
| 14 | `bms_temp` | integer °C | `bluetoothBms.getTempCelsius()` (max across sensors) | no BMS temp data |
| 15 | `cell_voltage_min_mv` | integer mV | `bluetoothBms.getCellMinMilliVolts()` | no BMS cell data |
| 16 | `cell_voltage_max_mv` | integer mV | `bluetoothBms.getCellMaxMilliVolts()` | no BMS cell data |
| 17 | `remaining_min` | integer minutes | `remainingTime.getRemainingMin()` — remaining **motor** time, updated every 10 s | no estimate (`RemainingTimeQuality::Unavailable`) |

### Appending fields

Fields are **appended at the end only**; fields 1–16 never move or change
meaning. Consumers must accept trailing fields they do not know: fly-app's
`xctod_parser.dart` accepts ≥ 16 fields since the release that preceded
field 17 (fly-app branch `fix/xctod-accept-trailing-fields`; release it
before this firmware), and XCTrack reads by index, so a new field never
shifts an existing widget. Field 17 is a voltage-based approximation when
no current is measured — the sentence has no way to say so; the Fly Control
`TELEMETRY` characteristic carries the quality.

### Signal validity

Fields 3, 8, and 12 (voltage, motor temp, ESC temp) each go blank
individually when their underlying `SignalState`
(`src/Telemetry/SignalState.h`) is not `Valid` — i.e. `Stale`, `Invalid`,
or `Absent` — instead of showing a possibly stale or implausible reading.
This mirrors the same per-signal validity check the Fly Control
`TELEMETRY.signalStates` field uses.

### `system_status` (field 13)

| Value | Meaning |
|-------|---------|
| `ARMED` | system is armed |
| `DISARMED` | not armed, no fault (`DisarmReason::None` or `Manual`) |
| fault code | not armed due to a fault — one of `THR ERR`, `LINK ERR`, `BATT ERR` (older firmware also sent `MOT ERR`, `ESC ERR`, `MOT SRC`; see `src/DisarmReason.h`, `disarmReasonCode()`); all fault codes fit an 8-character field width |

### `motor_temp_src` (field 9)

Reports which sensor fed the (possibly blank) `motor_temp` reading in
field 8 — `can` (T-Motor ESC's CAN Status 5) or `ntc` (thermistor
fallback). Always blank on XAG builds, which have a single motor temp
source. Independent of the field 8 validity check: it's populated
whenever a source has been selected, even on a tick where the reading
itself reads invalid.

## Differences from the CSV log

The CSV log (`TelemetryLogger`) uses the exact same field order and
values for fields 1-12, 14-16 and 17 above (as CSV columns, same names). It
differs from the BLE sentence in four ways:

1. No `$XCTOD,` prefix / `\r\n`-terminated sentence framing — it's a plain
   CSV row, prefixed with a `timestamp` column by `Logger`.
2. No `system_status` column — the CSV only ever logs while armed
   (`Logger::log()` gates on `throttle.isArmed()`), so "armed" is implicit
   in a row existing at all.
3. An additional `disarm_reason` column, appended after
   `cell_voltage_max_mv`. It's blank on every regular 1 Hz row and
   populated only on one final row, written at the exact armed→disarmed
   transition via `Logger::logFinalLine()`, holding the same fault code
   table as BLE's `system_status` (or `MANUAL` for a manual disarm).
4. `remaining_min` (field 17 here) is the CSV's **last** column, after
   `disarm_reason`, so it sits at a different column position. It is
   blank on the final disarm row.

## Breaking changes

Inserting, removing or reordering a field is a breaking change for every
consumer reading by position — XCTrack `WExternalData` `index` values (see
`xctrack-pages.xcfg`), fly-app's parser, and any tool reading the CSV by
column index. Appending a field at the end is not, provided the consumers
accept trailing fields (fly-app does from the release before field 17:
branch `fix/xctod-accept-trailing-fields`, to be released before this
firmware).
