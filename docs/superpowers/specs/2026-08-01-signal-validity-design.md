# Signal Validity — Design

Date: 2026-08-01
Status: Approved

## Problem

The firmware has no single notion of "is this reading trustworthy right now". Three ad-hoc
mechanisms answer overlapping questions inconsistently, and each one fails unsafe:

1. **Temperature range checks live in the consumer.** `Power::calcMotorTempLimit()` and
   `calcEscTempLimit()` (`src/Power/Power.cpp:148`, `:165`) return `100` when the reading is
   outside `MOTOR_TEMP_MIN_VALID..MAX_VALID` — a broken NTC silently *disables* thermal
   protection with no indication anywhere.

2. **The throttle has no validity concept at all.** `Throttle::getThrottleRaw()`
   (`src/Throttle/Throttle.cpp:185`) clamps with `constrain(pinValueFiltered, min, max)`. A Hall
   sensor or wire that rails high clamps to `throttlePinMax` — **full throttle**.

3. **`telemetry.hasData()` does not mean the data is real.** `TmotorTelemetry::update()`
   (`src/Tmotor/TmotorTelemetry.cpp:38`) sets `cachedHasData = true` unconditionally, zeroing
   current/RPM/ESC temp when no CAN frame arrived. The facade then uses `0` as the "no value"
   sentinel to fall back to the BMS (`src/Telemetry/Telemetry.cpp:57`), which is ambiguous
   because 0 A is a legitimate reading.

### Latent defects found while designing

These are pre-existing bugs that this work fixes as a side effect. They are the concrete
justification for validating in the ADC domain and for owning the I2C error path.

**NaN in the temperature path.** `Temperature::readTemperature()` computes
`rt = (v * R) / (vcc - v)` with `vcc` hard-coded to 3.3 (`src/Temperature/Temperature.cpp:54`).
The ADS1115 runs at `GAIN_ONE` (VREF 4.096 V), so nothing prevents `v` from reading above 3.3
when the NTC is disconnected and the real rail sits at 3.35 V. Then `vcc - v` is negative, `rt`
is negative, and `logf(rt / r0)` on line 59 returns **NaN**. `(int32_t)(NaN * 1000.0)` is
undefined behaviour, and every comparison against NaN in `Power` is false — so the garbage
passes the existing range check as *valid* and reaches `map()`.

**The ADS1X15 library discards I2C errors.** `Adafruit_ADS1X15::readRegister` ignores the return
of `write()` and `read()` and returns the contents of `buffer`, which on a failed transaction
still holds the bytes from the previous read. Therefore:

- `readADC_SingleEnded()` has no error channel — on a bus failure it returns the previous
  conversion result as if it were a fresh reading. The `lastValue[]` fallback in
  `src/ADS1115/ADS1115.cpp:40` never even engages; the freeze happens one layer below it.
- `readADC_SingleEnded()` busy-waits in `while (!conversionComplete()) ;` with no timeout. If the
  stale buffer has bit 15 clear, this is an **infinite loop** that hangs `loop()`, including the
  ESC PWM refresh.

The practical consequence: if I2C dies at 70% throttle, the motor holds 70% forever and the
throttle stops responding, with no indication. This is more dangerous than a broken wire, because
the frozen value stays inside the calibrated band and no range check can catch it.

## The validity model

Four explicit states. Every signal has exactly one at any moment.

| State | Meaning | Example |
|---|---|---|
| `Absent` | the source does not exist in this build | RPM on an XAG build |
| `Stale` | the source exists but has not delivered for too long | no CAN frame for 1 s |
| `Invalid` | a reading arrived but it is physically impossible | NTC at −40 °C, Hall outside the calibrated band |
| `Valid` | trustworthy reading — zero is a legitimate value | — |

`TelemetryAvailability` answers `Absent` vs. everything else. It stays as the bottom layer of
this model and is not rewritten.

### Architecture: validity as a property of each producer

Each source owns its own validity rule, because the rules have nothing in common: an NTC is
invalid by physical range, a CAN field by staleness, the throttle by calibrated band plus I2C
health. A thin aggregator collects the states for `/api/telemetry`. Pure decision logic goes into
host-testable headers, matching the existing `ThrottleEngagementLogic`, `RemoteLinkLogic` and
`PowerAlertLogic` convention.

Rejected alternatives:

- **Centralized validity registry.** Centralizes knowledge that is not naturally central; becomes
  a `switch` over signal types — the same logic, moved away from the component that understands
  it.
- **`Reading<T>` = `{value, status}` on every getter.** Correct in theory (makes it impossible to
  read a value without seeing its status), but touches every facade getter and every consumer at
  once, in a firmware with a documented flash/heap squeeze (commit 61fa202). Large diff, high
  risk, and the consumers are only three: `Power`, `/api/telemetry`, `Xctod`.

## Throttle

Pure header `src/Throttle/ThrottleSignalLogic.h`, host-tested. Input
`(raw, readOk, min, max, nowMs, config)`, output:

```cpp
enum class ThrottleSignalAction { Ok, ForceZero, Disarm };
```

A sample is invalid when `!readOk` **or** the value is outside
`[min − 20% of range, max + 10% of range]`. The margin is asymmetric on purpose: reading low only
costs power, reading high is the side that becomes unintended acceleration.

| | Wired | Wireless |
|---|---|---|
| `debounceMs` | 0 | 500 (current remote behaviour) |
| `disarmMs` | 0 — disarms on detection | 3000 (current remote behaviour) |
| `recoveryMs` | n/a | 200 |
| `ForceZero` reachable | no | yes (current remote behaviour) |

**Wired tolerates nothing.** An invalid reading disarms immediately: the motor goes to
`ESC_MIN_PWM` and the pilot diagnoses on the ground. No fallback to a held value — that would
mask a real fault while flying on a stale command.

**Immediate is safe without a debounce counter, because validation runs on the filtered value.**
`Throttle` already maintains an 8-sample moving average, so a single spurious sample moves the
average by only 1/8 of its deviation. With a typical calibration (min 800, max 3000, range 2200,
upper limit 3220): at full throttle, one sample saturating at 4095 moves the average to 3137 —
inside the band. Two consecutive samples reach 3274 and trip. The moving average *is* the
debounce, and it costs nothing.

**Wireless keeps its current tolerance,** because the physics differ. A wired fault — cut
conductor, dead I2C — does not heal; waiting 3 s only leaves the pilot without throttle for
2 s longer. A radio dropout heals constantly (someone walks in front of the antenna, the pilot
turns). Unifying the numbers would drag one of the two to the wrong value. The unification is in
having **one state machine**, parameterized by source.

**`recoveryMs = 200` (wireless only)** — the invalid timer only resets after 200 ms of sustained
validity. Without it, a bad link delivering one isolated packet every 2.5 s never disarms. This is
the one behavioural change to the remote path beyond absorbing its logic.

**Absorbed:** `RemoteLinkLogic::computeFailsafe` and the disarm branch at `src/main.cpp:161` go
away. `RemoteLink` exposes `isLinkHealthy()` as the wireless source's `readOk`, and the decision
becomes the same on both paths.

### What does not change

`getThrottleRaw()` and `getThrottlePercentage()` are untouched. Forcing them to 0 on invalid was
considered and rejected: with immediate disarm the value no longer controls anything, keeping it
raw preserves the existing arming block for the reads-high fault, and it remains useful on screen
for diagnosis.

No extra guard in `setArmed()` either. If the pilot arms with a bad signal, the validity machine
disarms on the next cycle — the immediate fault beep *is* the diagnosis. A separate arming gate
would be duplicate logic reaching the same place.

## Power-limiting sensors

Applies to motor temperature, ESC temperature and battery voltage.

**Arming takes a snapshot of each signal's state, and that contract holds for the whole session.**
The pilot arms knowing what the system will and will not do for them, and the system may not
change that promise mid-flight.

| State at arming | Event | Action |
|---|---|---|
| `Valid` | goes invalid in flight | **disarm** + disarm tone + persistent on-screen warning |
| `Invalid` | — | signal disabled for the entire session: no limiting, no disarm, permanent badge on screen |

**Why disarm rather than alert.** An alert the pilot does not perceive is not a mitigation. The
pilot often flies with the screen off and cannot hear beeps over the motor. Engine-off in flight
is a real risk and this is a deliberate trade: the operator prefers a hard stop over flying on a
protection they believe is active and is not.

**Why no synthetic power limit.** Holding the last valid limit degenerates — for most of a flight
the motor is below the reduction threshold, so the last valid limit is 100% and the rule collapses
into a fixed ceiling with extra state to maintain. And between "no limiting" (100%) and "assume
worst case" (0%) there is no value any measurement justifies, because the measurement is precisely
what was lost. A fixed ceiling would be a guess wearing the costume of engineering, which is worse
than no limit at all in a safety system, because the operator trusts the number.

**Why an invalid-at-arm signal stays disabled even if it recovers.** Resuming would also mean
resuming its power to disarm — and a flaky NTC the pilot knowingly accepted would then cut the
motor in flight on its next oscillation. A snapshot with no exceptions removes that path and
leaves no edge cases.

## Detection per source

**NTC via ADS1115.** Circuit: fixed 10 kΩ to VCC, NTC to GND, VCC 3.3 V, `GAIN_ONE` (VREF
4.096 V), converted to 0–4095 by `convertTo12Bit`.

| Condition | Rt | V | counts |
|---|---|---|---|
| **disconnected** (Rt → ∞) | ∞ | 3.300 | **3299** |
| −40 °C | 290 k | 3.190 | 3189 |
| −20 °C | 86 k | 2.955 | 2954 |
| 0 °C | 30 k | 2.479 | 2478 |
| 25 °C | 10 k | 1.650 | 1650 |
| 60 °C | 2.8 k | 0.724 | 724 |
| 150 °C | 282 | 0.091 | 91 |
| **shorted** (Rt → 0) | 0 | 0.000 | **0** |

**Valid band: 91 to 2954 counts** (−20 °C to 150 °C). That leaves 345 counts of separation from
the open circuit and 91 from a dead short. The hot limit matches the existing
`MOTOR_TEMP_MAX_VALID`; only the cold limit changes.

Validation happens **in the count domain, before the Steinhart-Hart math** — not after, as today.
This eliminates the NaN defect at its source and replaces float work with an integer comparison.

**I2C.** `readOk` comes from our own probe (`Wire.beginTransmission(0x48)` +
`endTransmission()`, which returns 0 on ACK), because the library discards the error. Costs a few
hundred µs at 400 kHz and is deterministic. It covers every ADS channel — throttle and
temperatures alike.

**CAN.** Two tests, both required: **freshness** (no frame for the field within N ms → `Stale`)
and **plausibility** (same physical band, because an intact frame can still carry a garbage
field). A field is only declared invalid when **all** its sources have failed — on Tmotor, motor
temperature already has CAN plus NTC with fallback in `src/Tmotor/TmotorTelemetry.cpp:15`.
Exhausting redundancy first makes the hard case rare.

## Display-only data

RPM, current, BMS. No effect on power or arming. A card whose state is not `Valid` shows `—` with
a state badge, never a fake number. The `.status.stale` CSS already exists at
`src/WebServer/Pages/CommonStyles.h:264`.

## API

`/api/telemetry` replaces the `availability` object (`src/WebServer/ControllerWebServer.cpp:719`)
with per-field states using single-letter codes, because `StaticJsonDocument<2048>` is tight:

```json
"signals": { "motorTemp":"v", "escTemp":"i", "battV":"v", "current":"a", "rpm":"a", "throttle":"v" }
```

`v` = valid, `s` = stale, `i` = invalid, `a` = absent.

New latched `disarmReason` field, cleared only on re-arm. Manual button disarm records `Manual`;
signal faults record their specific cause. The fault-disarm warning panel reuses the `PowerAlert`
panel styling but **does not** auto-dismiss — it is the record the operator needs in order to
diagnose.

A distinct buzzer pattern marks fault disarm, separate from manual disarm.

## Xctod and the telemetry log

Both surfaces already use the right convention — an **empty field** — but with the wrong gate.
`Xctod` (`src/Xctod/Xctod.cpp:153`, `:194`, `:218`) and `TelemetryLogger`
(`src/TelemetryLogger/TelemetryLogger.cpp:72`, `:108`, `:132`) emit empty fields when
`!telemetry.hasData()`. That gate is global and, on Tmotor, always true — so in practice they
**never** emit empty. They always ship a number, including the synthetic zeros
`TmotorTelemetry::update()` writes into current, RPM and ESC temperature when no CAN frame
arrived.

For a forensic record that is the worst possible outcome: a fabricated 0 A is indistinguishable
from a measured 0 A, so a post-flight analysis cannot tell a stopped ESC from a silent CAN bus.

**The fix changes no format** — it swaps the global gate for per-field validity. Empty means not
`Valid`. No substitute zero on either surface.

### Log CSV — two new columns

An empty value says "not valid" but not *why*, and the log is the diagnostic record.

- **`signals`** — the single-letter codes concatenated in fixed order, e.g. `vvivaa`. Seven bytes
  per row including the comma: cheap for LittleFS and trivial to parse.
- **`disarmReason`** — empty on every row, filled only on the row where the disarm happens.

**Ordering requirement.** `Logger` closes the file on disarm. On a fault disarm the row carrying
the reason must be written **before** the close, otherwise the log ends without recording the
cause — which is the very data the log exists for.

### Xctod

Same empty-field rule. Worth noting that XCTRACK is the surface most likely to be in the pilot's
field of view in flight — more so than the web page, which was the argument for disarming rather
than merely alerting.

`writeSystemStatus` (`src/Xctod/Xctod.cpp:229`) currently sends only `YES`/`NO`. It could carry
the disarm reason so the pilot sees on the instrument why the motor stopped. **This is not yet
safe to assert**: the format is consumed by a third-party app and it is unknown whether its parser
tolerates an extra field or a value other than `YES`/`NO`. Verify against XCTRACK before
implementing. If it does not tolerate it, the field stays as is and the reason appears only on the
web page and in the log.

## Testing

Host tests compiled with `c++ -std=c++17`, following `test/PowerTest.cpp`.

`test/ThrottleSignalLogicTest.cpp`
- valid reading passes
- out of band high, out of band low
- `readOk` false
- single saturated sample does not trip (moving-average debounce)
- two consecutive saturated samples do trip
- wireless: 500 ms debounce, 3 s disarm
- wireless: flapping link does not reset the timer (`recoveryMs`)

`test/SignalValidityTest.cpp`
- NTC count band at both extremes, plus open-circuit and short-circuit counts
- CAN freshness and plausibility
- all four cells of the arming-contract matrix
- invalid-at-arm signal that recovers stays disabled

`test/RemoteLinkLogicTest.cpp` is absorbed, since `computeFailsafe` ceases to exist.

Formatting of the `Xctod` and CSV output is verified by inspection on hardware rather than by host
test, since both are string assembly over the same validity states already covered above.

## Phasing

Two independent slices, separate PRs.

1. **Throttle** — `ThrottleSignalLogic`, `ADS1115::lastReadOk`, latched `DisarmReason`, on-screen
   warning, absorption of `RemoteLinkLogic`.
2. **Sensors and outputs** — count-domain validation, arming contract, disarm on loss, badges and
   the `signals` object in the API, per-field gating in `Xctod` and `TelemetryLogger`, the two new
   CSV columns.

## To verify during implementation

- Whether the XCTRACK parser tolerates the disarm reason in the system-status field. If not, the
  field stays `YES`/`NO`.
