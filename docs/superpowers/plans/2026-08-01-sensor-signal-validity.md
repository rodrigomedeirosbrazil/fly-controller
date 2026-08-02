# Sensor Signal Validity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Motor temperature, ESC temperature, and battery voltage each get a real validity check (physically-impossible-range detection in the ADC/mV domain, plus I2C health) instead of the current ad-hoc range check inside `Power`. A signal valid when the pilot arms and later invalid mid-flight disarms the system (the pilot was promised a protection and it silently vanishing is worse than a hard stop); a signal already invalid at arming time is disabled for the whole session (no limiting, no disarm) since the pilot knowingly accepted flying without it. Display-only fields (motor/ESC temp, battery voltage) show `—` with a badge instead of a fabricated number when not valid.

**Architecture:** Extends the validity model from `docs/superpowers/specs/2026-08-01-signal-validity-design.md` (already implemented for the throttle in the previous plan) to the three power-limiting sensors. Two small pure, host-testable classes carry the new logic: `SensorReadingValidity` (fixed-band + I2C-fail-streak check, reused by `Temperature` and `BatteryVoltageSensor` — the sensor-specific analogue of `ThrottleWiredValidity`, but with static bounds instead of calibration-relative ones) and `SignalArmContract` (the "valid at arm, stays valid or disarms; invalid at arm, stays disabled" decision, owned by `Power`, one instance per signal).

**Tech Stack:** ESP32-C3 Arduino/PlatformIO (C++17), host-native tests via `c++ -std=c++17`, ArduinoJson, vanilla JS.

**Scope note:** This is the sensor half of the design's phase 2 ("Sensors and outputs"). It covers count/mV-domain validation, the arm-time contract, disarm-on-loss, and the telemetry API/UI. It deliberately does **not** cover the CSV log's two new columns or Xctod's empty-field gating and system-status code — those are lower-priority, purely mechanical (string assembly over states this plan already produces), and are left for a follow-up plan so this one stays a coherent, independently-testable unit. It also keeps the existing `availability` JSON object as-is (still used for RPM/current/BMS display-omission) rather than replacing it — the new `signals` object is additive, covering only the three power-limiting fields this plan touches. Battery voltage's `SignalState` is based solely on the ADS1115 divider sensor; it does not consider the Bluetooth BMS as a redundant source (that pairing wasn't part of the approved design and adds complexity out of scope here).

---

## File Structure

| File | Responsibility |
|---|---|
| `src/Telemetry/SignalState.h` (new) | Shared four-state enum (`Absent`/`Stale`/`Invalid`/`Valid`) + single-letter API code |
| `src/ADS1115/SensorReadingValidity.h` (new) | Pure fixed-band + I2C-fail-streak validity check, reused by `Temperature` and `BatteryVoltageSensor` |
| `src/Power/SignalArmContract.h` (new) | Pure "valid-at-arm contract" decision, one instance per power-limiting signal, owned by `Power` |
| `src/Temperature/Temperature.h`, `.cpp` | Adds `ReadOkFn`, NTC count-domain valid band, `isValid()` |
| `src/Sensors/BatteryVoltageSensor.h`, `.cpp` | Adds `ReadOkFn`, mV-domain valid band, `isValid()` |
| `src/config.cpp` | Wires the new `ReadOkFn` lambdas for `motorTemp`, `escTemp`, `batterySensor` |
| `src/Tmotor/TmotorTelemetry.h`, `.cpp` | Real `hasData()` (fixes it always reporting true regardless of CAN freshness), motor/ESC temp `SignalState` (CAN+NTC redundancy for motor, CAN-only for ESC), battery voltage `SignalState` |
| `src/Xag/XagTelemetry.h`, `.cpp` | Motor/ESC temp/battery voltage `SignalState` (NTC/ADS1115-only, no CAN) |
| `src/Telemetry/TelemetryBackend.h` | Three more function-pointer slots for the new `SignalState` getters |
| `src/Telemetry/Telemetry.h`, `.cpp` | Facade dispatch for the three new getters + `isXValid()` convenience booleans |
| `src/Power/Power.h`, `.cpp` | Three `SignalArmContract` members, `onArmed()`, rewritten `calcMotorTempLimit`/`calcEscTempLimit`/`calcBatteryLimit` |
| `src/Throttle/Throttle.cpp` | Calls `power.onArmed()` on a successful arm |
| `src/WebServer/ControllerWebServer.cpp` | Adds a `signals` object to `/api/telemetry` |
| `src/WebServer/Pages/TelemetryPage.h` | Badges on the three cards; fault-disarm panel gains `MOT ERR`/`ESC ERR`/`BATT ERR` entries |
| `test/SignalStateTest.cpp`, `test/SensorReadingValidityTest.cpp`, `test/SignalArmContractTest.cpp` (new) | Host tests for the three new pure pieces |
| `CLAUDE.md`, `src/CLAUDE.md`, `docs/MANUAL-DE-USO.md`, `docs/MANUAL-INTERFACE-WEB.md` | Doc updates |

---

### Task 1: `SignalState` shared enum

**Files:**
- Create: `src/Telemetry/SignalState.h`
- Test: `test/SignalStateTest.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// test/SignalStateTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Telemetry/SignalState.h"
using namespace std;

void test_codes_match_table() {
    assert(signalStateCode(SignalState::Absent) == 'a');
    assert(signalStateCode(SignalState::Stale) == 's');
    assert(signalStateCode(SignalState::Invalid) == 'i');
    assert(signalStateCode(SignalState::Valid) == 'v');
    cout << "PASS: codes match the design table\n";
}

int main() {
    test_codes_match_table();
    cout << "SignalStateTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 test/SignalStateTest.cpp -o /tmp/signal_state_test && /tmp/signal_state_test`
Expected: FAIL to compile — the header doesn't exist yet.

- [ ] **Step 3: Write the header**

```cpp
// src/Telemetry/SignalState.h
#pragma once
#include <stdint.h>

// Four-state signal validity model — see
// docs/superpowers/specs/2026-08-01-signal-validity-design.md.
//   Absent  — the source doesn't exist in this build (e.g. RPM on XAG)
//   Stale   — the source exists but hasn't delivered recently
//   Invalid — a reading arrived but is physically impossible
//   Valid   — trustworthy; zero is a legitimate value
enum class SignalState : uint8_t { Absent, Stale, Invalid, Valid };

// Single-letter code used identically across /api/telemetry's `signals`
// object and (later) the CSV log.
inline char signalStateCode(SignalState s) {
    switch (s) {
        case SignalState::Absent:  return 'a';
        case SignalState::Stale:   return 's';
        case SignalState::Invalid: return 'i';
        case SignalState::Valid:   return 'v';
    }
    return 'a';
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 test/SignalStateTest.cpp -o /tmp/signal_state_test && /tmp/signal_state_test`
Expected: `SignalStateTest: all passed`

- [ ] **Step 5: Commit**

```bash
git add src/Telemetry/SignalState.h test/SignalStateTest.cpp
git commit -m "feat: add shared SignalState enum and API codes"
```

---

### Task 2: `SensorReadingValidity` — reusable fixed-band + I2C-streak check

**Files:**
- Create: `src/ADS1115/SensorReadingValidity.h`
- Test: `test/SensorReadingValidityTest.cpp`

This is the same shape as `ThrottleWiredValidity` (Task 6 of the previous plan), but with **static** bounds instead of calibration-relative ones — the NTC divider and the battery voltage divider both have fixed physical valid ranges, unlike the throttle, which is calibrated per unit.

- [ ] **Step 1: Write the failing test**

```cpp
// test/SensorReadingValidityTest.cpp
#include <cassert>
#include <iostream>
#include "../src/ADS1115/SensorReadingValidity.h"
using namespace std;

void test_in_band_reading_is_valid() {
    SensorReadingValidity v;
    assert(v.isValid(1650, 91, 2954));  // 25C, well inside the NTC band
    cout << "PASS: in-band reading is valid\n";
}

void test_below_band_is_invalid() {
    SensorReadingValidity v;
    assert(!v.isValid(90, 91, 2954));
    cout << "PASS: one count below the band is invalid\n";
}

void test_above_band_is_invalid() {
    SensorReadingValidity v;
    assert(!v.isValid(2955, 91, 2954));
    cout << "PASS: one count above the band is invalid\n";
}

void test_band_edges_are_valid() {
    SensorReadingValidity v;
    assert(v.isValid(91, 91, 2954));
    assert(v.isValid(2954, 91, 2954));
    cout << "PASS: the band edges themselves are valid (inclusive)\n";
}

void test_i2c_streak_tolerates_up_to_two_consecutive_failures() {
    SensorReadingValidity v;
    v.recordSample(false);
    assert(v.isValid(1650, 91, 2954));
    v.recordSample(false);
    assert(v.isValid(1650, 91, 2954));
    cout << "PASS: up to 2 consecutive I2C failures don't affect validity\n";
}

void test_i2c_streak_of_three_invalidates_on_the_same_tick() {
    SensorReadingValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(false);
    assert(!v.isValid(1650, 91, 2954));
    cout << "PASS: the 3rd consecutive I2C failure invalidates on the same tick\n";
}

void test_i2c_streak_resets_on_a_good_read() {
    SensorReadingValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(true);
    v.recordSample(false);
    assert(v.isValid(1650, 91, 2954));
    cout << "PASS: a good read resets the failure streak\n";
}

void test_reset_clears_the_streak() {
    SensorReadingValidity v;
    v.recordSample(false);
    v.recordSample(false);
    v.recordSample(false);
    assert(!v.isValid(1650, 91, 2954));
    v.reset();
    assert(v.isValid(1650, 91, 2954));
    cout << "PASS: reset() clears the failure streak\n";
}

int main() {
    test_in_band_reading_is_valid();
    test_below_band_is_invalid();
    test_above_band_is_invalid();
    test_band_edges_are_valid();
    test_i2c_streak_tolerates_up_to_two_consecutive_failures();
    test_i2c_streak_of_three_invalidates_on_the_same_tick();
    test_i2c_streak_resets_on_a_good_read();
    test_reset_clears_the_streak();
    cout << "SensorReadingValidityTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 test/SensorReadingValidityTest.cpp -o /tmp/srv_test && /tmp/srv_test`
Expected: FAIL to compile — the header doesn't exist yet.

- [ ] **Step 3: Write the header**

```cpp
// src/ADS1115/SensorReadingValidity.h
#pragma once

// Pure sensor-reading validity check — no Arduino deps, host-testable.
// Reused by Temperature and BatteryVoltageSensor: both are ADS1115-backed
// sensors with a *fixed* physical valid range (unlike the throttle, whose
// valid range is calibration-relative — see ThrottleWiredValidity for that
// version).
//
// "Valid" requires two independent things, matching the throttle's own
// wired-validity philosophy (see ThrottleWiredValidity.h):
//   1. The reading falls inside [lowBound, highBound] (inclusive).
//   2. The I2C read hasn't failed 3 times in a row. This is a single
//      un-averaged bool per tick, unlike the reading itself (which each
//      caller typically averages over several samples) — a lone
//      conversion-timeout or bus NAK is a plausible transient, not proof of
//      a dead sensor, so it gets a small tolerance before it counts against
//      validity.
//
// The unit of `value`/`lowBound`/`highBound` is whatever domain the caller
// validates in (raw ADC counts for the NTC divider, millivolts for the
// battery divider) — this class doesn't care, it's a plain band check.
class SensorReadingValidity {
public:
    enum : unsigned int { I2C_FAIL_STREAK_THRESHOLD = 3 };

    SensorReadingValidity() : i2cFailStreak_(0) {}

    void recordSample(bool readOk) {
        if (readOk) {
            i2cFailStreak_ = 0;
        } else {
            i2cFailStreak_++; // unsigned wraparound is safe and irrelevant
                               // at realistic timescales
        }
    }

    void reset() { i2cFailStreak_ = 0; }

    bool isValid(int value, int lowBound, int highBound) const {
        bool inBand = value >= lowBound && value <= highBound;
        bool i2cOk = i2cFailStreak_ < I2C_FAIL_STREAK_THRESHOLD;
        return inBand && i2cOk;
    }

private:
    unsigned int i2cFailStreak_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 test/SensorReadingValidityTest.cpp -o /tmp/srv_test && /tmp/srv_test`
Expected: `SensorReadingValidityTest: all passed`

- [ ] **Step 5: Commit**

```bash
git add src/ADS1115/SensorReadingValidity.h test/SensorReadingValidityTest.cpp
git commit -m "feat: add SensorReadingValidity — reusable fixed-band sensor validity check"
```

---

### Task 3: `SignalArmContract` — the arm-time contract decision

**Files:**
- Create: `src/Power/SignalArmContract.h`
- Test: `test/SignalArmContractTest.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// test/SignalArmContractTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Power/SignalArmContract.h"
using namespace std;

void test_valid_at_arm_limits_while_still_valid() {
    SignalArmContract c;
    c.onArmed(true);
    assert(c.shouldLimit(true));
    cout << "PASS: valid at arm, still valid -> limiting applies\n";
}

void test_valid_at_arm_then_invalid_disarms() {
    SignalArmContract c;
    c.onArmed(true);
    assert(c.shouldDisarmOnLoss(false));
    cout << "PASS: valid at arm, now invalid -> disarm\n";
}

void test_valid_at_arm_then_invalid_stops_limiting_too() {
    SignalArmContract c;
    c.onArmed(true);
    assert(!c.shouldLimit(false));
    cout << "PASS: valid at arm, now invalid -> no longer limits (disarm handles the motor)\n";
}

void test_invalid_at_arm_never_limits_even_if_it_recovers() {
    SignalArmContract c;
    c.onArmed(false);
    assert(!c.shouldLimit(true));
    cout << "PASS: invalid at arm -> disabled all session, even after recovering\n";
}

void test_invalid_at_arm_never_disarms() {
    SignalArmContract c;
    c.onArmed(false);
    assert(!c.shouldDisarmOnLoss(false));
    assert(!c.shouldDisarmOnLoss(true));
    cout << "PASS: invalid at arm -> never triggers a disarm from this signal\n";
}

void test_still_valid_never_triggers_disarm() {
    SignalArmContract c;
    c.onArmed(true);
    assert(!c.shouldDisarmOnLoss(true));
    cout << "PASS: still valid -> no disarm\n";
}

void test_rearming_takes_a_fresh_snapshot() {
    SignalArmContract c;
    c.onArmed(true);
    assert(c.shouldDisarmOnLoss(false)); // goes invalid, disarms
    c.onArmed(false); // pilot re-arms with the sensor still bad
    assert(!c.shouldLimit(true));        // disabled all session now
    assert(!c.shouldDisarmOnLoss(false));
    cout << "PASS: a fresh onArmed() call replaces the previous session's contract\n";
}

int main() {
    test_valid_at_arm_limits_while_still_valid();
    test_valid_at_arm_then_invalid_disarms();
    test_valid_at_arm_then_invalid_stops_limiting_too();
    test_invalid_at_arm_never_limits_even_if_it_recovers();
    test_invalid_at_arm_never_disarms();
    test_still_valid_never_triggers_disarm();
    test_rearming_takes_a_fresh_snapshot();
    cout << "SignalArmContractTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 test/SignalArmContractTest.cpp -o /tmp/sac_test && /tmp/sac_test`
Expected: FAIL to compile — the header doesn't exist yet.

- [ ] **Step 3: Write the header**

```cpp
// src/Power/SignalArmContract.h
#pragma once

// Pure "arm-time contract" decision for a single power-limiting signal —
// no Arduino deps, host-testable. See
// docs/superpowers/specs/2026-08-01-signal-validity-design.md, "Power-limiting
// sensors" section.
//
// The pilot arms knowing what protection is currently active. A signal
// invalid at arming time stays disabled for the whole session, even if it
// recovers — a recovered-then-refaulting sensor would otherwise cut the
// motor in flight on a protection the pilot knowingly accepted going
// without. A signal valid at arming time that later goes invalid is a
// promise broken mid-flight — that disarms.
//
// This class only decides; it does not gate itself on "are we currently
// armed" — the caller (Power) only ever asks these questions while armed,
// since Power::getPwm() is itself only invoked while armed (see
// main.cpp's handleEsc()). onArmed() is called exactly once per arm, taking
// a fresh snapshot each time.
class SignalArmContract {
public:
    SignalArmContract() : validAtArm_(false) {}

    // Call once, at the moment arming succeeds, with the signal's current
    // validity.
    void onArmed(bool validNow) { validAtArm_ = validNow; }

    // True if this signal should currently be used for power limiting.
    bool shouldLimit(bool validNow) const { return validAtArm_ && validNow; }

    // True if a disarm should fire right now (valid at arm, invalid now).
    bool shouldDisarmOnLoss(bool validNow) const { return validAtArm_ && !validNow; }

private:
    bool validAtArm_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 test/SignalArmContractTest.cpp -o /tmp/sac_test && /tmp/sac_test`
Expected: `SignalArmContractTest: all passed`

- [ ] **Step 5: Commit**

```bash
git add src/Power/SignalArmContract.h test/SignalArmContractTest.cpp
git commit -m "feat: add SignalArmContract — valid-at-arm decision for power-limiting sensors"
```

---

### Task 4: `Temperature` gets an `isValid()` in the ADC count domain

**Files:**
- Modify: `src/Temperature/Temperature.h`
- Modify: `src/Temperature/Temperature.cpp`

No new host test here — `Temperature` itself is Arduino-coupled (no existing test infra for it, same situation as `Throttle.cpp`); the pure decision it delegates to (`SensorReadingValidity`) is already tested in Task 2. Verified by compiling both build targets.

- [ ] **Step 1: Modify `src/Temperature/Temperature.h`**

```cpp
#ifndef Temperature_h
#define Temperature_h

#include "../ADS1115/SensorReadingValidity.h"

class Temperature
{
    public:
        typedef int (*ReadFn)();
        typedef bool (*ReadOkFn)();
        Temperature(ReadFn readFn, ReadOkFn readOkFn, float adcVoltageRef);
        void handle();
        double getTemperature() { return temperature; }
        bool isValid() const { return valid; }

    private:
        const double beta = 3600.0;
        const double r0 = 10000.0; // Resistance at T0
        const double t0 = 298.15;   // 25°C in Kelvin
        const double R = 10000.0;
        const static int samples = 10;

        // NTC divider valid band in raw ADC counts (0-4095), corresponding
        // to -20..150°C. See
        // docs/superpowers/specs/2026-08-01-signal-validity-design.md,
        // "Detection per source". Checked in the count domain, before the
        // Steinhart-Hart math, so a disconnected (reads near 4095) or
        // shorted (reads near 0) sensor is caught before the math below can
        // turn it into NaN.
        static const int NTC_VALID_COUNTS_LOW = 91;    // 150°C
        static const int NTC_VALID_COUNTS_HIGH = 2954; // -20°C

        ReadFn readFn;
        ReadOkFn readOkFn;
        float adcVoltageRef;
        int pinValues[samples];
        double temperature;
        bool valid;
        SensorReadingValidity validity;
        unsigned long lastPinRead;

        void readTemperature();
};

#endif
```

- [ ] **Step 2: Modify `src/Temperature/Temperature.cpp`**

```cpp
#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

Temperature::Temperature(ReadFn readFn, ReadOkFn readOkFn, float adcVoltageRef)
    : readFn(readFn), readOkFn(readOkFn) {
  this->adcVoltageRef = adcVoltageRef;

  memset(
    &pinValues,
    0,
    sizeof(pinValues[0]) * samples
  );

  temperature = 0;
  valid = false;
  lastPinRead = 0;
}

void Temperature::handle()
{
  unsigned long now = millis();

  if (now - lastPinRead < 100) {
    return;
  }

  lastPinRead = now;
  readTemperature();
}

void Temperature::readTemperature() {
  memmove(
    &pinValues,
    &pinValues[1],
    sizeof(pinValues[0]) * (samples - 1)
  );

  int oversampledValue = readFn();
  pinValues[samples - 1] = oversampledValue;
  validity.recordSample(readOkFn());

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }
  int averagedCounts = sum / samples;

  valid = validity.isValid(averagedCounts, NTC_VALID_COUNTS_LOW, NTC_VALID_COUNTS_HIGH);

  // Voltage at the divider point: ReadFn returns 0-4095, scale by adcVoltageRef
  float v = (adcVoltageRef * (float)sum) / (samples * 4095.0f);

  // Solving for rt: rt = (v * R) / (VCC - v)
  // VCC is 3.3V (sensor power supply)
  const float vcc = 3.3f;
  float rt = (v * R) / (vcc - v);

  // Steinhart-Hart equation using Beta coefficient:
  // 1/T = 1/T0 + (1/B) * ln(Rt/R0)
  // T = 1 / (1/T0 + (1/B) * ln(Rt/R0))
  float invT = (1.0f / t0) + (1.0f / beta) * logf(rt / r0);
  float tempK = 1.0f / invT;
  temperature = tempK - 273.15f;
}
```

Note: `temperature` is still computed even when `valid` is false (including the pre-existing NaN case when the sensor is disconnected and `v` exceeds `vcc`) — this class exposes validity as a separate property rather than gating the value itself, matching the project's stated architecture ("validity as a property of each producer"). Every consumer added in later tasks checks `isValid()` before using `getTemperature()`, so the NaN case becomes unreachable in practice even though the underlying computation is unchanged.

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: FAIL — `config.cpp`'s `Temperature` instantiations still use the old 2-argument constructor. Confirm the only errors are about the `Temperature` constructor call; fixed in Task 6.

- [ ] **Step 4: Commit**

```bash
git add src/Temperature/Temperature.h src/Temperature/Temperature.cpp
git commit -m "feat: Temperature gets a real isValid() in the ADC count domain"
```

---

### Task 5: `BatteryVoltageSensor` gets an `isValid()` in the millivolt domain

**Files:**
- Modify: `src/Sensors/BatteryVoltageSensor.h`
- Modify: `src/Sensors/BatteryVoltageSensor.cpp`

No new host test here, same reasoning as Task 4.

- [ ] **Step 1: Modify `src/Sensors/BatteryVoltageSensor.h`**

```cpp
#ifndef BATTERY_VOLTAGE_SENSOR_H
#define BATTERY_VOLTAGE_SENSOR_H

#include <stdint.h>
#include "../ADS1115/SensorReadingValidity.h"

class BatteryVoltageSensor {
public:
    typedef int (*ReadFn)();
    typedef bool (*ReadOkFn)();
    BatteryVoltageSensor(ReadFn readFn, ReadOkFn readOkFn, float dividerRatio, float adcVoltageRef);
    void handle();
    uint16_t getVoltageMilliVolts() const { return voltageMilliVolts; }
    void setDividerRatio(float ratio) { dividerRatio = ratio; }
    bool isValid() const { return valid; }

private:
    static constexpr unsigned long READ_INTERVAL_MS = 500;
    static constexpr float EMA_ALPHA = 0.3f;

    // Physically implausible battery-voltage bounds, in millivolts. A
    // disconnected divider reads near 0V (R2 pulls the tap to ground with no
    // path from the battery once R1 opens); no real 14S LiPo pack reads
    // above ~65.8V even during an abusive overcharge. The upper bound is
    // also capped below the uint16_t ceiling (65535) that
    // voltageMilliVolts's storage type imposes, so it stays reachable —
    // see docs/superpowers/specs/2026-08-01-signal-validity-design.md.
    static const int VALID_MIN_MV = 5000;
    static const int VALID_MAX_MV = 65000;

    ReadFn readFn;
    ReadOkFn readOkFn;
    float dividerRatio;
    float adcVoltageRef;
    uint16_t voltageMilliVolts;
    bool valid;
    SensorReadingValidity validity;
    unsigned long lastRead;
    float emaVoltageMilliVolts;
    bool emaInitialized;

    void readVoltage();
};

#endif // BATTERY_VOLTAGE_SENSOR_H
```

- [ ] **Step 2: Modify `src/Sensors/BatteryVoltageSensor.cpp`**

```cpp
#include "BatteryVoltageSensor.h"
#include "../config.h"

BatteryVoltageSensor::BatteryVoltageSensor(ReadFn readFn, ReadOkFn readOkFn, float dividerRatio, float adcVoltageRef)
    : readFn(readFn), readOkFn(readOkFn), dividerRatio(dividerRatio), adcVoltageRef(adcVoltageRef),
      voltageMilliVolts(0), valid(false), lastRead(0), emaVoltageMilliVolts(0.0f), emaInitialized(false) {
}

void BatteryVoltageSensor::handle() {
    unsigned long now = millis();
    if (now - lastRead < READ_INTERVAL_MS) {
        return;
    }
    lastRead = now;
    readVoltage();
}

void BatteryVoltageSensor::readVoltage() {
    int adcValue = readFn();
    validity.recordSample(readOkFn());

    // Convert ADC reading to voltage at sensor pin
    // adcVoltageRef: 3.3V for ESP32 ADC, 4.096V for ADS1115 GAIN_ONE
    double voltageAtPin = (adcVoltageRef * (double)adcValue) / ADC_MAX_VALUE;

    // Calculate actual battery voltage using divider ratio
    // V_battery = V_pin * BATTERY_DIVIDER_RATIO
    double batteryVoltage = voltageAtPin * dividerRatio;

    // Convert to millivolts
    // Maximum expected: 60.0V, so 60.0 * 1000 = 60000 mV < 65535 (uint16_t max)
    float rawMilliVolts = (float)(batteryVoltage * 1000.0);

    if (!emaInitialized) {
        emaVoltageMilliVolts = rawMilliVolts;
        emaInitialized = true;
    } else {
        emaVoltageMilliVolts += EMA_ALPHA * (rawMilliVolts - emaVoltageMilliVolts);
    }

    voltageMilliVolts = (uint16_t)(emaVoltageMilliVolts + 0.5f);
    valid = validity.isValid((int)voltageMilliVolts, VALID_MIN_MV, VALID_MAX_MV);
}
```

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: FAIL — `config.cpp`'s `BatteryVoltageSensor` instantiations still use the old 3-argument constructor, and `Temperature`'s from Task 4 too. Confirm the only errors are about these two constructors; fixed in Task 6.

- [ ] **Step 4: Commit**

```bash
git add src/Sensors/BatteryVoltageSensor.h src/Sensors/BatteryVoltageSensor.cpp
git commit -m "feat: BatteryVoltageSensor gets a real isValid() in the millivolt domain"
```

---

### Task 6: Wire the new `ReadOkFn` lambdas in `config.cpp`

**Files:**
- Modify: `src/config.cpp`

- [ ] **Step 1: Update all three `Temperature`/`BatteryVoltageSensor` instantiation blocks**

Replace the entire `#if IS_XAG ... #elif IS_TMOTOR ... #else ... #endif` block (currently defining `batterySensor`, `motorTemp`, `xagTelemetry`, `escTemp`) with:

```cpp
#if IS_XAG
BatteryVoltageSensor batterySensor(
    []() { return ads1115.readChannel(ADS1115_BATTERY_CHANNEL); },
    []() -> bool { return ads1115.lastReadOk(ADS1115_BATTERY_CHANNEL); },
    BATTERY_DIVIDER_RATIO,
    4.096f  // ADS1115 reference (GAIN_ONE)
);
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    []() -> bool { return ads1115.lastReadOk(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
XagTelemetry xagTelemetry;
Temperature escTemp(
    []() { return ads1115.readChannel(ADS1115_ESC_TEMP_CHANNEL); },
    []() -> bool { return ads1115.lastReadOk(ADS1115_ESC_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#elif IS_TMOTOR
BatteryVoltageSensor batterySensor(
    []() { return ads1115.readChannel(ADS1115_BATTERY_CHANNEL); },
    []() -> bool { return ads1115.lastReadOk(ADS1115_BATTERY_CHANNEL); },
    BATTERY_DIVIDER_RATIO,
    4.096f  // ADS1115 reference (GAIN_ONE)
);
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    []() -> bool { return ads1115.lastReadOk(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#else
Temperature motorTemp(
    []() { return ads1115.readChannel(ADS1115_MOTOR_TEMP_CHANNEL); },
    []() -> bool { return ads1115.lastReadOk(ADS1115_MOTOR_TEMP_CHANNEL); },
    4.096f  // ADS1115 reference (GAIN_ONE)
);
#endif
```

- [ ] **Step 2: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both environments build with no errors. This is the first checkpoint where the whole tree compiles again after Tasks 4-5's intentional breakage.

- [ ] **Step 3: Commit**

```bash
git add src/config.cpp
git commit -m "feat: wire I2C-health ReadOkFn for motorTemp, escTemp, batterySensor"
```

---

### Task 7: `TmotorTelemetry` — real `hasData()`, motor/ESC temp `SignalState`

**Files:**
- Modify: `src/Tmotor/TmotorTelemetry.h`
- Modify: `src/Tmotor/TmotorTelemetry.cpp`

- [ ] **Step 1: Modify `src/Tmotor/TmotorTelemetry.h`**

```cpp
#ifndef TmotorTelemetry_h
#define TmotorTelemetry_h

#include <stdint.h>
#include <Arduino.h>
#include "../Telemetry/SignalState.h"

/**
 * Tmotor telemetry aggregator: battery voltage from ADS1115; current/RPM/ESC temp from TmotorCan (CAN).
 * Motor temperature: CAN (Status 5 / PUSHCAN) when recently received; else NTC/ADS1115 fallback (ESC may not send motor temp).
 */
class TmotorTelemetry {
public:
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const;
    uint16_t getRpm() const;
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;

    SignalState getMotorTempState() const;
    SignalState getEscTempState() const;
    SignalState getBatteryVoltageState() const;

private:
    bool cachedHasData = false;
    uint16_t cachedBatteryVoltageMilliVolts = 0;
    uint32_t cachedBatteryCurrentMilliAmps = 0;
    uint16_t cachedRpm = 0;
    int32_t cachedMotorTempMilliCelsius = 0;
    int32_t cachedEscTempMilliCelsius = 0;
    unsigned long cachedLastUpdate = 0;
    SignalState cachedMotorTempState = SignalState::Invalid;
    SignalState cachedEscTempState = SignalState::Invalid;
};

#endif
```

- [ ] **Step 2: Modify `src/Tmotor/TmotorTelemetry.cpp`**

```cpp
#include "TmotorTelemetry.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"

extern TmotorCan tmotorCan;
extern Temperature motorTemp;
extern BatteryVoltageSensor batterySensor;

static int32_t motorTempMilliCelsiusForDisplay() {
    if (settings.getMotorTempSource() == MotorTempSourceAds1115) {
        return (int32_t)(motorTemp.getTemperature() * 1000.0);
    }
    // CAN (default): prefer CAN if recent, fall back to ADS1115
    if (tmotorCan.hasRecentMotorTempFromCan()) {
        return (int32_t)tmotorCan.getMotorTemperature() * 1000;
    }
    return (int32_t)(motorTemp.getTemperature() * 1000.0);
}

// Redundant sources: CAN (Status 5 / PUSHCAN), freshness + plausibility, or
// the NTC fallback. Invalid only when BOTH have failed — see
// docs/superpowers/specs/2026-08-01-signal-validity-design.md, "Detection
// per source". tmotorCan.getMotorTemperature() is a uint8_t Celsius value
// (unsigned, so it can't represent a too-cold reading — only the hot
// backstop applies here), compared against the existing millicelsius-domain
// MOTOR_TEMP_MAX_VALID converted to whole Celsius.
static SignalState motorTempStateForDisplay() {
    bool canOk = tmotorCan.hasRecentMotorTempFromCan() &&
                 tmotorCan.getMotorTemperature() <= (MOTOR_TEMP_MAX_VALID / 1000);
    if (canOk || motorTemp.isValid()) return SignalState::Valid;
    return SignalState::Invalid;
}

// CAN-only (no NTC on the ESC for Tmotor): stale when no fresh ESC_STATUS
// frame at all, invalid when the frame is fresh but the value is
// implausible.
static SignalState escTempStateForDisplay() {
    if (!tmotorCan.hasTelemetry()) return SignalState::Stale;
    if (tmotorCan.getEscTemperature() > (ESC_TEMP_MAX_VALID / 1000)) return SignalState::Invalid;
    return SignalState::Valid;
}

void TmotorTelemetry::update() {
    cachedBatteryVoltageMilliVolts = batterySensor.getVoltageMilliVolts();

    if (tmotorCan.hasTelemetry()) {
        cachedBatteryCurrentMilliAmps = tmotorCan.getBatteryCurrent();
        cachedRpm = tmotorCan.getRpm();
        cachedMotorTempMilliCelsius = motorTempMilliCelsiusForDisplay();
        cachedEscTempMilliCelsius = (int32_t)tmotorCan.getEscTemperature() * 1000;
    } else {
        // No ESC_STATUS recently: still expose battery (ADS1115), motor temp (CAN motor temp if fresh, else NTC)
        cachedBatteryCurrentMilliAmps = 0;
        cachedRpm = 0;
        cachedMotorTempMilliCelsius = motorTempMilliCelsiusForDisplay();
        cachedEscTempMilliCelsius = 0;
    }
    // Reflects real CAN freshness now, not a hardcoded true — a stale link
    // no longer masquerades as "has data" (see the design doc's problem #3).
    cachedHasData = tmotorCan.hasTelemetry();
    cachedLastUpdate = millis();

    cachedMotorTempState = motorTempStateForDisplay();
    cachedEscTempState = escTempStateForDisplay();
}

bool TmotorTelemetry::hasData() const { return cachedHasData; }
uint16_t TmotorTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
uint32_t TmotorTelemetry::getBatteryCurrentMilliAmps() const { return cachedBatteryCurrentMilliAmps; }
uint16_t TmotorTelemetry::getRpm() const { return cachedRpm; }
int32_t TmotorTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t TmotorTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long TmotorTelemetry::getLastUpdate() const { return cachedLastUpdate; }
SignalState TmotorTelemetry::getMotorTempState() const { return cachedMotorTempState; }
SignalState TmotorTelemetry::getEscTempState() const { return cachedEscTempState; }
SignalState TmotorTelemetry::getBatteryVoltageState() const {
    return batterySensor.isValid() ? SignalState::Valid : SignalState::Invalid;
}
#endif
```

**Important consequence to be aware of:** `cachedHasData` no longer being hardcoded `true` means `isRpmAvailable()`/`isCurrentAvailable()`/`isPowerKwAvailable()` (in `src/Telemetry/TelemetryAvailability.cpp`, unchanged by this plan) now correctly reflect real CAN staleness instead of always reporting available whenever `hasCurrentSensor` is true. This is an intended fix, not a side effect to work around — it directly resolves the design doc's problem #3 ("`telemetry.hasData()` does not mean the data is real").

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: `lolin_c3_mini_xag` SUCCESS (unaffected, XAG excludes `Tmotor/` from the build). `lolin_c3_mini_tmotor` FAILS — `TelemetryBackend`'s struct literal in `Telemetry.cpp` doesn't yet have the three new function pointers this class now exposes methods for; that mismatch doesn't actually cause a build error yet (the new methods just aren't called by anything), so in practice this should **also** build successfully already. Run the build and confirm; if it unexpectedly fails, the only acceptable cause is a typo in this task's own code — stop and report if you see anything else.

- [ ] **Step 4: Commit**

```bash
git add src/Tmotor/TmotorTelemetry.h src/Tmotor/TmotorTelemetry.cpp
git commit -m "fix: TmotorTelemetry.hasData() reflects real CAN freshness; add motor/ESC temp SignalState"
```

---

### Task 8: `XagTelemetry` — motor/ESC temp/battery voltage `SignalState`

**Files:**
- Modify: `src/Xag/XagTelemetry.h`
- Modify: `src/Xag/XagTelemetry.cpp`

- [ ] **Step 1: Modify `src/Xag/XagTelemetry.h`**

```cpp
#ifndef XagTelemetry_h
#define XagTelemetry_h

#include <stdint.h>
#include <Arduino.h>
#include "../Telemetry/SignalState.h"

/**
 * XAG telemetry aggregator: motorTemp, escTemp, batterySensor (all ADC/ADS1115)
 */
class XagTelemetry {
public:
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const { return 0; }
    uint16_t getRpm() const { return 0; }
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;

    SignalState getMotorTempState() const;
    SignalState getEscTempState() const;
    SignalState getBatteryVoltageState() const;

private:
    bool cachedHasData = false;
    uint16_t cachedBatteryVoltageMilliVolts = 0;
    int32_t cachedMotorTempMilliCelsius = 0;
    int32_t cachedEscTempMilliCelsius = 0;
    unsigned long cachedLastUpdate = 0;
};

#endif
```

- [ ] **Step 2: Modify `src/Xag/XagTelemetry.cpp`**

```cpp
#include "XagTelemetry.h"
#include "../config_controller.h"
#if IS_XAG
#include "../config.h"

extern Temperature motorTemp;
extern Temperature escTemp;
extern BatteryVoltageSensor batterySensor;

void XagTelemetry::update() {
    cachedBatteryVoltageMilliVolts = batterySensor.getVoltageMilliVolts();
    cachedMotorTempMilliCelsius = (int32_t)(motorTemp.getTemperature() * 1000.0);
    cachedEscTempMilliCelsius = (int32_t)(escTemp.getTemperature() * 1000.0);
    cachedLastUpdate = millis();
    cachedHasData = true;
}

bool XagTelemetry::hasData() const { return cachedHasData; }
uint16_t XagTelemetry::getBatteryVoltageMilliVolts() const { return cachedBatteryVoltageMilliVolts; }
int32_t XagTelemetry::getMotorTempMilliCelsius() const { return cachedMotorTempMilliCelsius; }
int32_t XagTelemetry::getEscTempMilliCelsius() const { return cachedEscTempMilliCelsius; }
unsigned long XagTelemetry::getLastUpdate() const { return cachedLastUpdate; }
SignalState XagTelemetry::getMotorTempState() const { return motorTemp.isValid() ? SignalState::Valid : SignalState::Invalid; }
SignalState XagTelemetry::getEscTempState() const { return escTemp.isValid() ? SignalState::Valid : SignalState::Invalid; }
SignalState XagTelemetry::getBatteryVoltageState() const { return batterySensor.isValid() ? SignalState::Valid : SignalState::Invalid; }
#endif
```

(`cachedHasData` stays hardcoded `true` for XAG deliberately — there is no CAN/staleness concept on this build at all; motor/ESC temp/battery are always physically present, and their individual `SignalState` getters above are what actually carries validity now.)

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both SUCCEED.

- [ ] **Step 4: Commit**

```bash
git add src/Xag/XagTelemetry.h src/Xag/XagTelemetry.cpp
git commit -m "feat: XagTelemetry exposes motor/ESC temp/battery voltage SignalState"
```

---

### Task 9: `TelemetryBackend` + `Telemetry` facade — dispatch the new getters

**Files:**
- Modify: `src/Telemetry/TelemetryBackend.h`
- Modify: `src/Telemetry/Telemetry.h`
- Modify: `src/Telemetry/Telemetry.cpp`

- [ ] **Step 1: Modify `src/Telemetry/TelemetryBackend.h`**

```cpp
#ifndef TELEMETRY_BACKEND_H
#define TELEMETRY_BACKEND_H

#include <stdint.h>
#include <stdbool.h>
#include "SignalState.h"

/**
 * Telemetry backend interface: function pointers for runtime dispatch.
 * Eliminates repetitive #if IS_TMOTOR / #elif IS_XAG in Telemetry.cpp.
 */
struct TelemetryBackend {
    void (*update)(void);
    bool (*hasData)(void);
    uint16_t (*getBatteryVoltageMilliVolts)(void);
    uint32_t (*getBatteryCurrentMilliAmps)(void);
    uint16_t (*getRpm)(void);
    int32_t (*getMotorTempMilliCelsius)(void);
    int32_t (*getEscTempMilliCelsius)(void);
    unsigned long (*getLastUpdate)(void);
    SignalState (*getMotorTempState)(void);
    SignalState (*getEscTempState)(void);
    SignalState (*getBatteryVoltageState)(void);
};

#endif // TELEMETRY_BACKEND_H
```

- [ ] **Step 2: Modify `src/Telemetry/Telemetry.h`**

```cpp
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include <stdbool.h>
#include "SignalState.h"

/**
 * Unified telemetry facade: delegates to TmotorTelemetry or XagTelemetry.
 * Provides stable getter API for Power, BatteryMonitor, Xctod.
 */
class Telemetry {
public:
    void init();
    void update();

    bool hasData() const;
    uint16_t getBatteryVoltageMilliVolts() const;
    uint32_t getBatteryCurrentMilliAmps() const;
    uint16_t getRpm() const;
    int32_t getMotorTempMilliCelsius() const;
    int32_t getEscTempMilliCelsius() const;
    unsigned long getLastUpdate() const;

    SignalState getMotorTempState() const;
    SignalState getEscTempState() const;
    SignalState getBatteryVoltageState() const;
    bool isMotorTempValid() const { return getMotorTempState() == SignalState::Valid; }
    bool isEscTempValid() const { return getEscTempState() == SignalState::Valid; }
    bool isBatteryVoltageValid() const { return getBatteryVoltageState() == SignalState::Valid; }
};

extern Telemetry telemetry;

#endif // TELEMETRY_H
```

- [ ] **Step 3: Modify `src/Telemetry/Telemetry.cpp`**

```cpp
#include "Telemetry.h"
#include "TelemetryBackend.h"
#include "../config_controller.h"
#include "../config.h"

static const TelemetryBackend* s_backend_ptr = nullptr;

#if IS_TMOTOR
static void wrapUpdate() { tmotorTelemetry.update(); }
static bool wrapHasData() { return tmotorTelemetry.hasData(); }
static uint16_t wrapGetBatteryVoltageMilliVolts() { return tmotorTelemetry.getBatteryVoltageMilliVolts(); }
static uint32_t wrapGetBatteryCurrentMilliAmps() { return tmotorTelemetry.getBatteryCurrentMilliAmps(); }
static uint16_t wrapGetRpm() { return tmotorTelemetry.getRpm(); }
static int32_t wrapGetMotorTempMilliCelsius() { return tmotorTelemetry.getMotorTempMilliCelsius(); }
static int32_t wrapGetEscTempMilliCelsius() { return tmotorTelemetry.getEscTempMilliCelsius(); }
static unsigned long wrapGetLastUpdate() { return tmotorTelemetry.getLastUpdate(); }
static SignalState wrapGetMotorTempState() { return tmotorTelemetry.getMotorTempState(); }
static SignalState wrapGetEscTempState() { return tmotorTelemetry.getEscTempState(); }
static SignalState wrapGetBatteryVoltageState() { return tmotorTelemetry.getBatteryVoltageState(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate,
    wrapGetMotorTempState, wrapGetEscTempState, wrapGetBatteryVoltageState
};
#elif IS_XAG
static void wrapUpdate() { xagTelemetry.update(); }
static bool wrapHasData() { return xagTelemetry.hasData(); }
static uint16_t wrapGetBatteryVoltageMilliVolts() { return xagTelemetry.getBatteryVoltageMilliVolts(); }
static uint32_t wrapGetBatteryCurrentMilliAmps() { return xagTelemetry.getBatteryCurrentMilliAmps(); }
static uint16_t wrapGetRpm() { return xagTelemetry.getRpm(); }
static int32_t wrapGetMotorTempMilliCelsius() { return xagTelemetry.getMotorTempMilliCelsius(); }
static int32_t wrapGetEscTempMilliCelsius() { return xagTelemetry.getEscTempMilliCelsius(); }
static unsigned long wrapGetLastUpdate() { return xagTelemetry.getLastUpdate(); }
static SignalState wrapGetMotorTempState() { return xagTelemetry.getMotorTempState(); }
static SignalState wrapGetEscTempState() { return xagTelemetry.getEscTempState(); }
static SignalState wrapGetBatteryVoltageState() { return xagTelemetry.getBatteryVoltageState(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate,
    wrapGetMotorTempState, wrapGetEscTempState, wrapGetBatteryVoltageState
};
#endif

void Telemetry::init() {
#if IS_TMOTOR || IS_XAG
    s_backend_ptr = &s_backend;
#endif
}

void Telemetry::update() {
    if (s_backend_ptr && s_backend_ptr->update) {
        s_backend_ptr->update();
    }
}

bool Telemetry::hasData() const {
    if (s_backend_ptr && s_backend_ptr->hasData && s_backend_ptr->hasData()) {
        return true;
    }
    return bluetoothBms.hasData();
}

uint16_t Telemetry::getBatteryVoltageMilliVolts() const {
    uint16_t v = s_backend_ptr && s_backend_ptr->getBatteryVoltageMilliVolts ? s_backend_ptr->getBatteryVoltageMilliVolts() : 0;
    if (v == 0 && bluetoothBms.hasData()) {
        return (uint16_t)bluetoothBms.getPackVoltageMilliVolts();
    }
    return v;
}

uint32_t Telemetry::getBatteryCurrentMilliAmps() const {
    uint32_t a = s_backend_ptr && s_backend_ptr->getBatteryCurrentMilliAmps ? s_backend_ptr->getBatteryCurrentMilliAmps() : 0;
    if (a == 0 && bluetoothBms.hasData()) {
        int32_t bmsMa = bluetoothBms.getPackCurrentMilliAmps();
        return (uint32_t)(bmsMa < 0 ? -bmsMa : bmsMa);
    }
    return a;
}

uint16_t Telemetry::getRpm() const {
    return s_backend_ptr && s_backend_ptr->getRpm ? s_backend_ptr->getRpm() : 0;
}

int32_t Telemetry::getMotorTempMilliCelsius() const {
    return s_backend_ptr && s_backend_ptr->getMotorTempMilliCelsius ? s_backend_ptr->getMotorTempMilliCelsius() : 0;
}

int32_t Telemetry::getEscTempMilliCelsius() const {
    return s_backend_ptr && s_backend_ptr->getEscTempMilliCelsius ? s_backend_ptr->getEscTempMilliCelsius() : 0;
}

unsigned long Telemetry::getLastUpdate() const {
    return s_backend_ptr && s_backend_ptr->getLastUpdate ? s_backend_ptr->getLastUpdate() : 0;
}

SignalState Telemetry::getMotorTempState() const {
    return s_backend_ptr && s_backend_ptr->getMotorTempState ? s_backend_ptr->getMotorTempState() : SignalState::Absent;
}

SignalState Telemetry::getEscTempState() const {
    return s_backend_ptr && s_backend_ptr->getEscTempState ? s_backend_ptr->getEscTempState() : SignalState::Absent;
}

SignalState Telemetry::getBatteryVoltageState() const {
    return s_backend_ptr && s_backend_ptr->getBatteryVoltageState ? s_backend_ptr->getBatteryVoltageState() : SignalState::Absent;
}

Telemetry telemetry;
```

- [ ] **Step 4: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both SUCCEED.

- [ ] **Step 5: Commit**

```bash
git add src/Telemetry/TelemetryBackend.h src/Telemetry/Telemetry.h src/Telemetry/Telemetry.cpp
git commit -m "feat: Telemetry facade dispatches SignalState for motor/ESC temp/battery voltage"
```

---

### Task 10: `Power` owns the arm-time contract, replaces the old ad-hoc range checks

**Files:**
- Modify: `src/Power/Power.h`
- Modify: `src/Power/Power.cpp`

- [ ] **Step 1: Modify `src/Power/Power.h`**

```cpp
#ifndef POWER_H
#define POWER_H

#include <Arduino.h>
#include "SignalArmContract.h"

enum PowerLimitCause : uint8_t {
    POWER_LIMIT_NONE       = 0,
    POWER_LIMIT_BATTERY    = 1 << 0,
    POWER_LIMIT_MOTOR_TEMP = 1 << 1,
    POWER_LIMIT_ESC_TEMP   = 1 << 2,
};

class Throttle;
class Temperature;

class Power {
public:
    Power();
    unsigned int getPwm();
    unsigned int getPower();
    uint8_t getActiveLimitCauses() const { return activeLimitCauses_; }
    void resetBatteryPowerFloor();
    void resetMotorState();

    // Snapshots each power-limiting signal's current validity for the
    // arm-time contract. Called by Throttle::setArmed() on a successful
    // arm — see SignalArmContract.h.
    void onArmed();

private:
    enum class StartState {
        IDLE,
        STARTING,
        RUNNING,
    };

    long lastPowerCalculationTime;
    unsigned int power;
    unsigned int batteryPowerFloor;

    StartState startState;
    unsigned long startingBeganAt;
    unsigned long idleBeganAt;

    uint8_t activeLimitCauses_;

    SignalArmContract motorTempContract_;
    SignalArmContract escTempContract_;
    SignalArmContract batteryContract_;

    unsigned int calcPower();
    unsigned int calcBatteryLimit();
    unsigned int calcMotorTempLimit();
    unsigned int calcEscTempLimit();
};

#endif // POWER_H
```

- [ ] **Step 2: Modify `src/Power/Power.cpp`**

```cpp
#include "Power.h"
#include "../config.h"
#include "../BoardConfig.h"
#include "../DisarmReason.h"
#include "../Throttle/Throttle.h"

extern Throttle throttle;
extern Settings settings;

Power::Power() {
    lastPowerCalculationTime = 0;
    power = 100;
    batteryPowerFloor = 100;
    activeLimitCauses_ = POWER_LIMIT_NONE;
    startState = StartState::IDLE;
    startingBeganAt = 0;
    idleBeganAt = 0;
}

void Power::onArmed() {
    motorTempContract_.onArmed(telemetry.isMotorTempValid());
    escTempContract_.onArmed(telemetry.isEscTempValid());
    batteryContract_.onArmed(telemetry.isBatteryVoltageValid());
}

unsigned int Power::getPwm() {
    if (!throttle.isCalibrated()) {
        resetMotorState();
        return ESC_MIN_PWM;
    }

    unsigned int powerLimit  = getPower();
    unsigned int throttleMin = throttle.getThrottlePinMin();
    unsigned int throttleMax = throttle.getThrottlePinMax();
    unsigned int throttleRaw = throttle.isEngaged() ? throttle.getThrottleRaw() : throttleMin;

    unsigned int allowedMax = throttleMin + ((throttleMax - throttleMin) * powerLimit) / 100;
    unsigned int clampedRaw = constrain(throttleRaw, throttleMin, allowedMax);

    unsigned int range = throttleMax - throttleMin;
    float targetPwm;
    if (range == 0) {
        targetPwm = (float)ESC_MIN_PWM;
    } else {
        float norm = (float)(clampedRaw - throttleMin) / (float)range;
        targetPwm = (float)ESC_MIN_PWM + (float)(ESC_MAX_PWM - ESC_MIN_PWM) * norm;
    }
    targetPwm = constrain(targetPwm, (float)ESC_MIN_PWM, (float)ESC_MAX_PWM);

    bool throttleActive = (targetPwm > (float)(ESC_MIN_PWM + THROTTLE_DEADBAND_US));

    if (getBoardConfig().useSmoothStart) {
        unsigned long now = millis();

        switch (startState) {

        case StartState::IDLE:
            if (throttleActive) {
                startState = StartState::STARTING;
                startingBeganAt = now;
            }
            return ESC_MIN_PWM;

        case StartState::STARTING: {
            if (!throttleActive) {
                startState = StartState::IDLE;
                return ESC_MIN_PWM;
            }
            float wakeupPwm = (float)ESC_MIN_PWM + (float)(ESC_MAX_PWM - ESC_MIN_PWM) * (float)XAG_WAKEUP_PWM_PERCENT / 100.0f;
            if (now - startingBeganAt >= XAG_MOTOR_REACTION_DELAY_MS) {
                startState = StartState::RUNNING;
                return (unsigned int)targetPwm;
            }
            return (unsigned int)wakeupPwm;
        }

        case StartState::RUNNING:
            if (!throttleActive) {
                if (idleBeganAt == 0) idleBeganAt = now;

                if (now - idleBeganAt >= MOTOR_STOP_TIME_MS) {
                    startState = StartState::IDLE;
                    idleBeganAt = 0;
                    return ESC_MIN_PWM;
                }
            } else {
                idleBeganAt = 0;
            }
            return (unsigned int)targetPwm;
        }
    }

    return (unsigned int)targetPwm;
}

void Power::resetMotorState() {
    startState = StartState::IDLE;
    startingBeganAt = 0;
    idleBeganAt = 0;
}

unsigned int Power::getPower() {
    if (millis() - lastPowerCalculationTime < 500) {
        return power;
    }
    lastPowerCalculationTime = millis();
    power = calcPower();
    return power;
}

unsigned int Power::calcPower() {
    if (!settings.getPowerControlEnabled()) {
        activeLimitCauses_ = POWER_LIMIT_NONE;
        return 100;
    }
    unsigned int batteryLimit   = calcBatteryLimit();
    unsigned int motorTempLimit = calcMotorTempLimit();
    unsigned int escTempLimit   = calcEscTempLimit();

    uint8_t causes = POWER_LIMIT_NONE;
    if (batteryLimit < 100)   causes |= POWER_LIMIT_BATTERY;
    if (motorTempLimit < 100) causes |= POWER_LIMIT_MOTOR_TEMP;
    if (escTempLimit < 100)   causes |= POWER_LIMIT_ESC_TEMP;
    activeLimitCauses_ = causes;

    return min(min(batteryLimit, motorTempLimit), escTempLimit);
}

unsigned int Power::calcBatteryLimit() {
    if (!getBoardConfig().useBatteryLimit) return 100;

    bool validNow = telemetry.isBatteryVoltageValid();
    if (batteryContract_.shouldDisarmOnLoss(validNow)) {
        throttle.setDisarmed(DisarmReason::BatteryVoltageLost);
    }
    if (!batteryContract_.shouldLimit(validNow)) return 100;

    uint16_t batteryMilliVolts = telemetry.getBatteryVoltageMilliVolts();
    const unsigned int STEP_DECREASE = 5;

    if (batteryMilliVolts > settings.getBatteryMinVoltage()) {
        return batteryPowerFloor;
    }
    if (batteryPowerFloor < STEP_DECREASE) {
        batteryPowerFloor = 0;
        return 0;
    }
    batteryPowerFloor -= STEP_DECREASE;
    return batteryPowerFloor;
}

unsigned int Power::calcMotorTempLimit() {
    bool validNow = telemetry.isMotorTempValid();
    if (motorTempContract_.shouldDisarmOnLoss(validNow)) {
        throttle.setDisarmed(DisarmReason::MotorTempLost);
    }
    if (!motorTempContract_.shouldLimit(validNow)) return 100;

    int32_t motorTempMilliCelsius = telemetry.getMotorTempMilliCelsius();
    int32_t reductionStart = settings.getMotorTempReductionStart();
    int32_t maxTemp        = settings.getMotorMaxTemp();

    if (motorTempMilliCelsius < reductionStart) return 100;
    if (reductionStart == maxTemp) return 0;

    return constrain(map(motorTempMilliCelsius, reductionStart, maxTemp, 100, 0), 0, 100);
}

unsigned int Power::calcEscTempLimit() {
    bool validNow = telemetry.isEscTempValid();
    if (escTempContract_.shouldDisarmOnLoss(validNow)) {
        throttle.setDisarmed(DisarmReason::EscTempLost);
    }
    if (!escTempContract_.shouldLimit(validNow)) return 100;

    int32_t escTempMilliCelsius = telemetry.getEscTempMilliCelsius();
    int32_t reductionStart = settings.getEscTempReductionStart();
    int32_t maxTemp        = settings.getEscMaxTemp();

    if (escTempMilliCelsius < reductionStart) return 100;
    if (reductionStart == maxTemp) return 0;

    return constrain(map(escTempMilliCelsius, reductionStart, maxTemp, 100, 0), 0, 100);
}

void Power::resetBatteryPowerFloor() {
    batteryPowerFloor = 100;
    resetMotorState();
}
```

**Behavioral notes for the reviewer, not code changes:**
- `calcBatteryLimit()`'s old conservative-50%-before-first-CAN-frame floor is gone. It was gated on `telemetry.hasData()`, which was really a crude "has the system finished booting" proxy, not a real battery-specific check — and it's moot now regardless, since `calcBatteryLimit()` (like the other two `calc*Limit` functions) is only ever invoked via `Power::getPwm()`, which is only called from `main.cpp`'s `handleEsc()` while `throttle.isArmed()` — arming requires the 3-second throttle calibration sweep to finish first, by which point both the ADS1115 and CAN would have long since delivered their first reading. The uniform arm-time-contract check (`shouldLimit`) replaces it with something that reflects real sensor state instead of a timing guess.
- `calcPower()`'s `POWER_LIMIT_BATTERY` cause used to be additionally gated on `telemetry.hasData()`, specifically to exclude that startup floor from being reported as an active limiting cause. With the startup floor gone, `batteryLimit < 100` is now always a real limit, so the extra gate is removed.
- Detection latency for a sensor going invalid mid-flight is bounded by `getPower()`'s existing 500ms cache (`calcPower()`, and hence the disarm-on-loss checks, only actually run every 500ms) — the same cadence these three signals were already evaluated at before this change, so this isn't a new latency, just an existing one that now also gates a disarm decision.

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both SUCCEED.

- [ ] **Step 4: Commit**

```bash
git add src/Power/Power.h src/Power/Power.cpp
git commit -m "feat: Power owns the arm-time contract for motor/ESC temp and battery voltage"
```

---

### Task 11: `Throttle::setArmed()` snapshots the sensors on a successful arm

**Files:**
- Modify: `src/Throttle/Throttle.cpp`

- [ ] **Step 1: Add the `power.onArmed()` call**

In `src/Throttle/Throttle.cpp`'s `setArmed()`, right after `throttleArmed = true;` (the last line of the function):

```cpp
  signalLogic.reset();
  signalForcedZero = false;
  lastDisarmReason = DisarmReason::None;
  throttleArmed = true;
  power.onArmed();
}
```

(`power` is already an accessible extern in this file — `power.resetBatteryPowerFloor()` is already called earlier in this same function.)

- [ ] **Step 2: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both SUCCEED.

- [ ] **Step 3: Run the full host test suite**

```bash
for f in SignalStateTest SensorReadingValidityTest SignalArmContractTest DisarmReasonTest ThrottleSignalLogicTest ThrottleWiredValidityTest RemoteLinkFreshnessTest ThrottleEngagementLogicTest PowerAlertLogicTest PowerTest JkBmsParserTest RemoteLinkProtocolTest; do
  c++ -std=c++17 test/$f.cpp -o /tmp/$f && /tmp/$f || echo "FAILED: $f"
done
```

Expected: every binary prints its pass line, no `FAILED` lines. **`test/PowerTest.cpp` may need attention here** — it mocks `Throttle`/`Canbus`/`MotorTemp` as plain structs (per `test/CLAUDE.md`'s stated convention for this file) rather than including the real `Power.cpp`, so it should be unaffected by this plan's changes to the real `Power` class. Confirm it still compiles and passes as-is; if it references anything this plan renamed or removed, fix the test's mocks to match (but do not weaken any assertion to make it pass — if a mock genuinely needs new fields to compile, add them with realistic values).

- [ ] **Step 4: Commit**

```bash
git add src/Throttle/Throttle.cpp
git commit -m "feat: Throttle::setArmed() snapshots power-limiting sensor validity on a successful arm"
```

---

### Task 12: `/api/telemetry` gets a `signals` object

**Files:**
- Modify: `src/WebServer/ControllerWebServer.cpp`

- [ ] **Step 1: Add the `signals` object**

In the `/api/telemetry` handler, right after the existing `availability` object block (`availability["bmsCells"] = isBmsCellDataAvailable();`):

```cpp
        // Availability: explicit flags so frontend can show N/A vs 0
        JsonObject availability = doc.createNestedObject("availability");
        availability["current"] = isCurrentAvailable();
        availability["rpm"] = isRpmAvailable();
        availability["powerKw"] = isPowerKwAvailable();
        availability["bms"] = isBmsDataAvailable();
        availability["bmsCells"] = isBmsCellDataAvailable();

        // Signal validity for the three power-limiting sensors — see
        // docs/superpowers/specs/2026-08-01-signal-validity-design.md.
        JsonObject signals = doc.createNestedObject("signals");
        char motorTempCode[2] = { signalStateCode(telemetry.getMotorTempState()), '\0' };
        char escTempCode[2]   = { signalStateCode(telemetry.getEscTempState()), '\0' };
        char battVCode[2]     = { signalStateCode(telemetry.getBatteryVoltageState()), '\0' };
        signals["motorTemp"] = motorTempCode;
        signals["escTemp"] = escTempCode;
        signals["battV"] = battVCode;
```

Add the include near the other relative includes at the top of the file:

```cpp
#include "../Telemetry/SignalState.h"
```

(`SignalState.h` is likely already visible transitively through `config.h` → `Telemetry/Telemetry.h`, but include it explicitly for clarity, matching how `../DisarmReason.h` was explicitly included in the previous plan's Task 8 for the same reason.)

- [ ] **Step 2: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both SUCCEED.

- [ ] **Step 3: Commit**

```bash
git add src/WebServer/ControllerWebServer.cpp
git commit -m "feat: expose per-sensor SignalState via /api/telemetry's signals object"
```

---

### Task 13: Telemetry page — badges on the three cards, extend the fault-disarm panel

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h`

- [ ] **Step 1: Add badge spans to the three card labels**

Find and replace these three label lines (they currently read, respectively, at the battery/motor-temp/ESC-temp cards):

```html
                <div class="card" id="cardBattery">
                    <div class="label">Tens&#xE3;o</div>
```
→
```html
                <div class="card" id="cardBattery">
                    <div class="label">Tens&#xE3;o <span class="status" id="battVBadge" style="display:none;"></span></div>
```

```html
                <div class="card" id="cardMotorTemp">
                    <div class="label">Motor</div>
```
→
```html
                <div class="card" id="cardMotorTemp">
                    <div class="label">Motor <span class="status" id="motorTempBadge" style="display:none;"></span></div>
```

```html
                <div class="card" id="cardEscTemp">
                    <div class="label">ESC</div>
```
→
```html
                <div class="card" id="cardEscTemp">
                    <div class="label">ESC <span class="status" id="escTempBadge" style="display:none;"></span></div>
```

- [ ] **Step 2: Add the badge-rendering helper and wire it into `renderTelemetry`**

Add this near the top of the `<script>` section, right after the existing `setStatus` function definition:

```js
const SIGNAL_BADGE_TEXT = { s: 'DESATUALIZADO', i: 'INVÁLIDO', a: 'SEM DADO' };
const SIGNAL_BADGE_CLASS = { s: 'stale', i: 'nodata', a: 'status-secondary' };

// Shows formattedValue and hides the badge when the signal is valid;
// otherwise shows "—" and a badge describing why — never a fabricated
// number for a signal that isn't valid.
const renderSignalBadge = (badgeId, valueId, code, formattedValue) => {
    const badge = $(badgeId);
    const valueEl = $(valueId);
    if (!badge || !valueEl) return;

    if (!code || code === 'v') {
        badge.style.display = 'none';
        valueEl.textContent = formattedValue;
    } else {
        badge.style.display = '';
        badge.className = `status ${SIGNAL_BADGE_CLASS[code] || 'status-secondary'}`;
        badge.textContent = SIGNAL_BADGE_TEXT[code] || code;
        valueEl.textContent = '—'; // —
    }
};
```

Then, in `renderTelemetry`, replace these three lines:

```js
    setText('batteryVoltage', fmtV(data.batteryVoltageMv || 0));
```
and
```js
    setText('motorTemp', fmtC(data.motorTempMc || 0));
```
and
```js
    setText('escTemp', fmtC(data.escTempMc || 0));
```

with:

```js
    const signals = data.signals || {};
    renderSignalBadge('battVBadge', 'batteryVoltage', signals.battV, fmtV(data.batteryVoltageMv || 0));
    renderSignalBadge('motorTempBadge', 'motorTemp', signals.motorTemp, fmtC(data.motorTempMc || 0));
    renderSignalBadge('escTempBadge', 'escTemp', signals.escTemp, fmtC(data.escTempMc || 0));
```

(Leave every other `setText(...)` line in `renderTelemetry` exactly as-is — only these three lines change.)

- [ ] **Step 3: Extend the fault-disarm panel's reason lookup**

Find the `FAULT_DISARM_INFO` object (added in the previous plan) and add the three new entries:

```js
const FAULT_DISARM_INFO = {
    'THR ERR':  { title: 'Desarmado: falha no acelerador (com fio)', detail: 'Leitura fora da faixa calibrada ou falha de leitura do ADS1115.' },
    'LINK ERR': { title: 'Desarmado: falha no acelerador (sem fio)', detail: 'Link com o remote perdido por mais de 3 segundos.' },
    'MOT ERR':  { title: 'Desarmado: sensor de temperatura do motor', detail: 'A leitura de temperatura do motor, válida no momento em que o sistema foi armado, tornou-se inválida durante o voo.' },
    'ESC ERR':  { title: 'Desarmado: sensor de temperatura do ESC', detail: 'A leitura de temperatura do ESC, válida no momento em que o sistema foi armado, tornou-se inválida durante o voo.' },
    'BATT ERR': { title: 'Desarmado: sensor de tensão da bateria', detail: 'A leitura de tensão da bateria, válida no momento em que o sistema foi armado, tornou-se inválida durante o voo.' },
};
```

- [ ] **Step 4: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both SUCCEED (this file is a PROGMEM string literal — a syntax mistake inside the HTML/JS won't be caught by the C++ compiler beyond basic string literal validity, so also do a careful visual read-through of the final `TelemetryPage.h` for balanced braces/quotes in the sections you touched).

- [ ] **Step 5: Manual verification (not possible without hardware — note in report)**

Same as the previous plan's Task 9: physical bench verification (watching the badges actually appear/disappear on a real device as a sensor is disconnected) requires hardware this environment doesn't have. Do the best static trace you can: for each of the three signals, trace `renderSignalBadge` by hand for `code = 'v'` (hidden badge, real value shown), `code = 'i'` (red badge "INVÁLIDO", value shows "—"), `code = 's'` (yellow badge "DESATUALIZADO", value shows "—"), and `code = undefined` (older/malformed response — falls through to the `!code` branch, badge hidden, value shows the formatted value using `|| 0` fallbacks already in place upstream).

- [ ] **Step 6: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: show validity badges on motor/ESC temp/battery cards, extend fault-disarm reasons"
```

---

### Task 14: Documentation

**Files:**
- Modify: `CLAUDE.md`
- Modify: `src/CLAUDE.md`
- Modify: `docs/MANUAL-DE-USO.md`
- Modify: `docs/MANUAL-INTERFACE-WEB.md`

Read each file's current state before editing — this plan was written before the previous plan's doc changes were necessarily re-verified against the latest tree, and section numbers/exact wording may have shifted slightly.

- [ ] **Step 1: Update `CLAUDE.md`**

In the "Power & Safety Logic" section, extend the existing bullet list with a new item (find the existing bullets about battery voltage / motor temp / ESC temp progressive reduction and add this after them):

```markdown
- **Sensor validity & arm-time contract:** motor temp, ESC temp, and battery voltage each have a real validity check (`Temperature`/`BatteryVoltageSensor`'s `isValid()`, `SensorReadingValidity`, host-tested) instead of trusting every reading. Arming snapshots each signal's validity (`Power::onArmed()`, `SignalArmContract`, host-tested): valid at arm and later invalid mid-flight disarms (`DisarmReason::MotorTempLost`/`EscTempLost`/`BatteryVoltageLost`); invalid at arm disables that signal's limiting for the whole session, even if it recovers. See `docs/superpowers/specs/2026-08-01-signal-validity-design.md`.
```

- [ ] **Step 2: Update `src/CLAUDE.md`**

In the "### Temperature — `Temperature/`" entry, add a sentence:

```markdown
### Temperature — `Temperature/`
NTC thermistor via Steinhart-Hart (beta=3600, R0=10kΩ). Accepts `ReadFn` + `ReadOkFn` + `adcVoltageRef`. Multiple instances: `motorTemp` (all builds), `escTemp` (XAG only). `isValid()` checks the averaged raw ADC counts against a fixed physical band (91-2954, i.e. -20..150°C) plus I2C read health, via the shared `SensorReadingValidity` (`src/ADS1115/SensorReadingValidity.h`) — also used by `BatteryVoltageSensor`.
```

In the "### Sensors — `Sensors/`" entry, add a sentence:

```markdown
### Sensors — `Sensors/`
`BatteryVoltageSensor`: voltage divider via `ReadFn` + `ReadOkFn` + divider ratio + ADS1115 VREF. Used in XAG and Tmotor builds. `isValid()` checks the EMA-smoothed millivolt reading against a fixed plausible range (5000-65000 mV) plus I2C read health, via `SensorReadingValidity`.
```

In the "### Power — `Power/`" entry, add a sentence after the existing content:

```markdown
Owns the arm-time contract for the three power-limiting signals (motor temp, ESC temp, battery voltage) via `SignalArmContract` (`src/Power/SignalArmContract.h`, host-tested in `test/SignalArmContractTest.cpp`): `onArmed()` (called by `Throttle::setArmed()` on a successful arm) snapshots each signal's current validity. A signal valid at arm that goes invalid mid-flight disarms the system (`throttle.setDisarmed(DisarmReason::MotorTempLost)` etc.); a signal already invalid at arm has its limiting disabled for the whole session, even if it later reads valid again.
```

- [ ] **Step 3: Update `docs/MANUAL-DE-USO.md`**

Find section 6 ("Controle de potência pelos sensores", or whatever its current number is after the previous plan's renumbering — check the file yourself) and add a new subsection right after the "Comportamento geral" subsection, before the next `##` heading:

```markdown
### Sensor inválido ou perdido

Se a leitura de um sensor (temperatura do motor, temperatura do ESC ou tensão da bateria) for fisicamente impossível (por exemplo, sensor desconectado ou falha de comunicação), o sistema trata isso de duas formas, dependendo de quando o problema aparece:

- **Sensor já inválido no momento em que você arma:** a proteção daquele sensor específico fica desabilitada durante todo o voo — a potência não é limitada por ele, mesmo que a leitura volte ao normal depois. A tela mostra um aviso permanente indicando qual sensor está com problema.
- **Sensor válido ao armar, e fica inválido durante o voo:** o sistema **desarma automaticamente**, com o mesmo alarme sonoro contínuo e aviso permanente na tela usados para falha do acelerador. Isso porque uma proteção que você esperava estar ativa deixou de funcionar no meio do voo, sem aviso — o sistema prefere parar o motor a continuar voando com uma proteção que parou de existir silenciosamente.

Em ambos os casos, para voltar a operar normalmente, corrija o problema do sensor e desarme/arme novamente.
```

- [ ] **Step 4: Update `docs/MANUAL-INTERFACE-WEB.md`**

In section 9 ("Observações técnicas"), extend the `/api/telemetry` paragraph (the same one the previous plan extended with `disarmReason`) with a sentence about the new `signals` object:

```markdown
O objeto **signals** traz o estado de cada sensor que pode limitar a potência: `motorTemp`, `escTemp` e `battV`, cada um com um código de uma letra (`v` = válido, `s` = desatualizado, `i` = inválido, `a` = ausente). A página de Telemetria mostra um selo colorido e "—" no lugar do valor quando o código não é `v`.
```

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md src/CLAUDE.md docs/MANUAL-DE-USO.md docs/MANUAL-INTERFACE-WEB.md
git commit -m "docs: document sensor validity and the arm-time contract"
```

---

### Task 15: Final verification

**Files:** none (verification only)

- [ ] **Step 1: Full host test suite**

```bash
for f in SignalStateTest SensorReadingValidityTest SignalArmContractTest DisarmReasonTest ThrottleSignalLogicTest ThrottleWiredValidityTest RemoteLinkFreshnessTest ThrottleEngagementLogicTest PowerAlertLogicTest PowerTest JkBmsParserTest RemoteLinkProtocolTest; do
  c++ -std=c++17 -Wall -Wextra test/$f.cpp -o /tmp/$f && /tmp/$f || echo "FAILED: $f"
done
```

Expected: every test prints its pass line, no `FAILED` lines, no compiler warnings.

- [ ] **Step 2: Full firmware build, both targets**

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag
```

Expected: `SUCCESS` for both environments. Note the Flash usage percentage in the report — the project has a documented history of flash/heap pressure (commit 61fa202); if either target crosses 95%, flag it clearly even though it still builds.

- [ ] **Step 3: Bench verification (both builds) — requires physical hardware, not possible in this environment**

Document these as the manual checks a human needs to run before merging, rather than attempting them:

1. **Arm with all sensors healthy.** Confirm no badges appear, normal power limiting behavior (temp/voltage reduce power as before this plan).
2. **Disconnect the motor-temp NTC while armed and flying (or on the bench with the motor stopped).** Confirm: fault-disarm alarm plays, `MOT ERR` appears in the telemetry page's persistent warning panel, the motor-temp card shows a red "INVÁLIDO" badge and "—" instead of a temperature.
3. **Reconnect, re-arm.** Confirm the system arms normally and the warning clears.
4. **Disconnect the motor-temp NTC BEFORE arming, then arm anyway.** Confirm: the system arms successfully (no block), the motor-temp card shows the invalid badge, and — critically — the system does NOT disarm later in the session even if the NTC is reconnected and then disconnected again (invalid-at-arm stays disabled for the whole session, per the arm-time contract).
5. **Repeat steps 2-4 for ESC temperature and for battery voltage** (disconnecting/reconnecting the divider tap, or simulating via a bench power supply).
6. **On the Tmotor build specifically:** disconnect the CAN bus entirely while armed and confirm the RPM/current fields on the telemetry page switch to "N/A" within a few seconds (this exercises the `hasData()` fix from Task 7) rather than continuing to show a frozen last-known value.

- [ ] **Step 4: Report results to the user**

Summarize pass/fail for Steps 1-2 (which you can actually run) and clearly flag that Step 3 needs the user's hardware before this plan can be considered fully verified.
