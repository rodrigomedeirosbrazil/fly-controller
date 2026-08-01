# Throttle Signal Validity — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make an invalid throttle reading (wired: out-of-band or I2C failure; wireless: link loss) disarm the system instead of silently clamping to full throttle or riding a stale value, and record why the disarm happened so the pilot can diagnose it.

**Architecture:** A new pure, host-testable state machine (`ThrottleSignalLogic`) replaces both the implicit "just clamp" behavior of the wired path and the existing `RemoteLinkLogic::computeFailsafe` of the wireless path. `Throttle` owns one instance and feeds it a single `valid` boolean each read tick, parameterized per source (wired: zero tolerance; wireless: existing 500 ms/3 s timing). A `DisarmReason` shared enum records why the system disarmed, surfaced identically on the telemetry page and (later, phase 2) the CSV log and Xctod.

**Tech Stack:** ESP32-C3 Arduino/PlatformIO (C++17), host-native tests compiled with `c++ -std=c++17` (no PlatformIO device needed), ArduinoJson for the web API, vanilla JS for the telemetry page.

**Scope note:** This is phase 1 of `docs/superpowers/specs/2026-08-01-signal-validity-design.md` — the throttle-only slice. Phase 2 (power-limiting sensors, display-only signals, CSV log, Xctod) is a separate plan.

---

## File Structure

| File | Responsibility |
|---|---|
| `src/DisarmReason.h` (new) | Shared enum + short display code, one definition used everywhere a disarm reason is shown |
| `src/Throttle/ThrottleSignalLogic.h` (new) | Pure debounce/disarm/recovery state machine, no Arduino deps |
| `src/ADS1115/ADS1115.h`, `.cpp` | Adds `lastReadOk()` — a real I2C health signal, replacing the library's silent error-swallowing |
| `src/Buzzer/Buzzer.h`, `.cpp` | Adds `beepFaultDisarm()` — a pattern distinct from every existing beep |
| `src/RemoteLink/RemoteLink.h`, `.cpp` | Replaces `failsafe()`/`FailsafeAction` with `isLinkFresh()`, a raw per-tick freshness signal |
| `src/RemoteLink/RemoteLinkLogic.h` (delete) | Absorbed into `ThrottleSignalLogic` |
| `src/Throttle/Throttle.h`, `.cpp` | Owns the `ThrottleSignalLogic` instance, the latched `DisarmReason`, and the wired/wireless config selection |
| `src/config.cpp` | Throttle now takes two function pointers (value + validity) instead of one |
| `src/main.cpp` | Drops the old wireless-only failsafe-disarm block (absorbed into `Throttle::handle()`) |
| `src/WebServer/ControllerWebServer.cpp` | Adds `disarmReason` to `/api/telemetry` |
| `src/WebServer/Pages/TelemetryPage.h` | Non-dismissible fault-disarm warning panel |
| `test/ThrottleSignalLogicTest.cpp` (new) | Host test for the state machine |
| `test/DisarmReasonTest.cpp` (new) | Host test for the code table |
| `test/RemoteLinkLogicTest.cpp` (delete) | Covered by `ThrottleSignalLogicTest.cpp`'s wireless-config cases |
| `CLAUDE.md`, `src/CLAUDE.md`, `docs/MANUAL-DE-USO.md`, `docs/MANUAL-INTERFACE-WEB.md` | Doc updates so the shipped behavior matches what's documented |

---

### Task 1: `DisarmReason` shared enum

**Files:**
- Create: `src/DisarmReason.h`
- Test: `test/DisarmReasonTest.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// test/DisarmReasonTest.cpp
#include <cassert>
#include <cstring>
#include <iostream>
#include "../src/DisarmReason.h"
using namespace std;

void test_codes_match_table() {
    assert(strcmp(disarmReasonCode(DisarmReason::None), "") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::Manual), "MANUAL") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::ThrottleWiredInvalid), "THR ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::ThrottleLinkLost), "LINK ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::MotorTempLost), "MOT ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::EscTempLost), "ESC ERR") == 0);
    assert(strcmp(disarmReasonCode(DisarmReason::BatteryVoltageLost), "BATT ERR") == 0);
    cout << "PASS: codes match the design table\n";
}

void test_codes_fit_xctrack_field_width() {
    // XCTRACK's system-status field must hold every code, 8 chars max.
    const DisarmReason all[] = {
        DisarmReason::None, DisarmReason::Manual, DisarmReason::ThrottleWiredInvalid,
        DisarmReason::ThrottleLinkLost, DisarmReason::MotorTempLost,
        DisarmReason::EscTempLost, DisarmReason::BatteryVoltageLost,
    };
    for (DisarmReason r : all) {
        assert(strlen(disarmReasonCode(r)) <= 8);
    }
    cout << "PASS: every code fits the 8-char field width\n";
}

int main() {
    test_codes_match_table();
    test_codes_fit_xctrack_field_width();
    cout << "DisarmReasonTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 test/DisarmReasonTest.cpp -o /tmp/disarm_reason_test && /tmp/disarm_reason_test`
Expected: FAIL to compile — `src/DisarmReason.h` does not exist yet.

- [ ] **Step 3: Write the header**

```cpp
// src/DisarmReason.h
#pragma once
#include <stdint.h>

// Why the system disarmed. One definition, shared by the telemetry web page,
// the CSV log, and Xctod's system-status field (phase 2) — so a reason never
// drifts out of sync between surfaces.
//
// `None` is an internal sentinel only: the boot-time default, and what
// Throttle resets to on a successful arm. It is never meant to be shown next
// to an armed system; it exists so "never disarmed yet" is distinguishable
// from "disarmed manually".
enum class DisarmReason : uint8_t {
    None = 0,
    Manual = 1,
    ThrottleWiredInvalid = 2,
    ThrottleLinkLost = 3,
    MotorTempLost = 4,        // phase 2
    EscTempLost = 5,          // phase 2
    BatteryVoltageLost = 6,   // phase 2
};

// Short fixed-width code (<= 8 chars), identical across every surface that
// shows a disarm reason.
inline const char* disarmReasonCode(DisarmReason reason) {
    switch (reason) {
        case DisarmReason::None:                 return "";
        case DisarmReason::Manual:                return "MANUAL";
        case DisarmReason::ThrottleWiredInvalid:  return "THR ERR";
        case DisarmReason::ThrottleLinkLost:      return "LINK ERR";
        case DisarmReason::MotorTempLost:         return "MOT ERR";
        case DisarmReason::EscTempLost:           return "ESC ERR";
        case DisarmReason::BatteryVoltageLost:    return "BATT ERR";
    }
    return "";
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 test/DisarmReasonTest.cpp -o /tmp/disarm_reason_test && /tmp/disarm_reason_test`
Expected: `DisarmReasonTest: all passed`

- [ ] **Step 5: Commit**

```bash
git add src/DisarmReason.h test/DisarmReasonTest.cpp
git commit -m "feat: add shared DisarmReason enum and display codes"
```

---

### Task 2: `ThrottleSignalLogic` state machine

**Files:**
- Create: `src/Throttle/ThrottleSignalLogic.h`
- Test: `test/ThrottleSignalLogicTest.cpp`

- [ ] **Step 1: Write the failing test**

```cpp
// test/ThrottleSignalLogicTest.cpp
#include <cassert>
#include <iostream>
#include "../src/Throttle/ThrottleSignalLogic.h"
using namespace std;

// Wired: zero tolerance — any invalid sample disarms on the same tick.
static const ThrottleSignalConfig kWired{0, 0, 0};
// Wireless: matches the old RemoteLinkLogic timings, plus a 200 ms recovery
// guard so a flapping link cannot look "recovered" mid-episode.
static const ThrottleSignalConfig kWireless{500, 3000, 200};

void test_valid_stays_ok() {
    ThrottleSignalLogic logic;
    assert(logic.update(true, 0, kWired) == ThrottleSignalAction::Ok);
    assert(logic.update(true, 1000000, kWireless) == ThrottleSignalAction::Ok);
    cout << "PASS: valid reading always stays Ok\n";
}

void test_wired_disarms_on_first_invalid_sample() {
    ThrottleSignalLogic logic;
    assert(logic.update(true, 0, kWired) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 10, kWired) == ThrottleSignalAction::Disarm);
    cout << "PASS: wired disarms immediately, no tolerance\n";
}

void test_wireless_debounce_then_forcezero() {
    ThrottleSignalLogic logic;
    assert(logic.update(false, 0, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 499, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 500, kWireless) == ThrottleSignalAction::ForceZero);
    cout << "PASS: wireless tolerates up to the 500ms debounce before ForceZero\n";
}

void test_wireless_disarms_after_3s() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);
    assert(logic.update(false, 2999, kWireless) == ThrottleSignalAction::ForceZero);
    assert(logic.update(false, 3000, kWireless) == ThrottleSignalAction::Disarm);
    cout << "PASS: wireless disarms at the 3s mark\n";
}

void test_wireless_flapping_link_does_not_reset_the_clock() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);                                     // episode starts at t=0
    assert(logic.update(false, 500, kWireless) == ThrottleSignalAction::ForceZero);
    assert(logic.update(true,  600, kWireless) == ThrottleSignalAction::ForceZero);  // one good packet — not recovered yet (only 0ms sustained)
    assert(logic.update(false, 650, kWireless) == ThrottleSignalAction::ForceZero);  // back to bad before recovering
    // No further valid samples: episode clock has been running since t=0 the
    // whole time, unaffected by the single blip at t=600.
    assert(logic.update(false, 3000, kWireless) == ThrottleSignalAction::Disarm);
    cout << "PASS: a single-tick blip does not reset the invalid-episode clock\n";
}

void test_wireless_recovers_after_sustained_validity() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);
    assert(logic.update(false, 500, kWireless) == ThrottleSignalAction::ForceZero);
    // Valid from t=501 onward, sustained for >= 200ms (recoveryMs).
    assert(logic.update(true, 501, kWireless) == ThrottleSignalAction::ForceZero); // 0ms sustained
    assert(logic.update(true, 700, kWireless) == ThrottleSignalAction::ForceZero); // 199ms sustained
    assert(logic.update(true, 701, kWireless) == ThrottleSignalAction::Ok);        // 200ms sustained — recovered
    cout << "PASS: recovers to Ok after 200ms of sustained validity\n";
}

void test_recovery_actually_clears_the_episode() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWireless);
    logic.update(false, 500, kWireless);
    logic.update(true, 501, kWireless);
    logic.update(true, 701, kWireless); // recovered, per the previous test
    // A brand new invalid episode must restart from Ok, not resume mid-way.
    assert(logic.update(false, 750, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 1249, kWireless) == ThrottleSignalAction::Ok);
    assert(logic.update(false, 1250, kWireless) == ThrottleSignalAction::ForceZero);
    cout << "PASS: recovery fully resets the episode clock for the next fault\n";
}

void test_reset_clears_state() {
    ThrottleSignalLogic logic;
    logic.update(false, 0, kWired); // Disarm
    logic.reset();
    assert(logic.update(true, 1, kWired) == ThrottleSignalAction::Ok);
    cout << "PASS: reset() clears any in-progress episode\n";
}

int main() {
    test_valid_stays_ok();
    test_wired_disarms_on_first_invalid_sample();
    test_wireless_debounce_then_forcezero();
    test_wireless_disarms_after_3s();
    test_wireless_flapping_link_does_not_reset_the_clock();
    test_wireless_recovers_after_sustained_validity();
    test_recovery_actually_clears_the_episode();
    test_reset_clears_state();
    cout << "ThrottleSignalLogicTest: all passed" << endl;
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 test/ThrottleSignalLogicTest.cpp -o /tmp/throttle_signal_test && /tmp/throttle_signal_test`
Expected: FAIL to compile — `src/Throttle/ThrottleSignalLogic.h` does not exist yet.

- [ ] **Step 3: Write the state machine**

```cpp
// src/Throttle/ThrottleSignalLogic.h
#pragma once
#include <stdint.h>

// Pure throttle-signal validity state machine — no Arduino deps, host-testable.
//
// Feeds a per-tick "is this sample trustworthy" boolean through a
// debounce/disarm timer parameterized per source:
//   - Wired:    debounceMs=0, disarmMs=0 — the first invalid sample disarms.
//     No fallback to a held value: a wired fault does not heal on its own,
//     and riding a stale command while the pilot is unaware is worse than a
//     clean stop they can diagnose on the ground.
//   - Wireless: debounceMs=500, disarmMs=3000 — matches the previous
//     RemoteLinkLogic timings. A radio dropout heals constantly (the pilot
//     turns, someone steps clear of the antenna), so it keeps its existing
//     tolerance via the ForceZero intermediate state.
//
// recoveryMs guards against a flapping signal: the invalid-episode clock only
// resets after recoveryMs of *consecutive* valid samples. Without it, a link
// delivering one isolated good packet every couple of seconds would look
// "recovered" on every good packet and never reach Disarm.
struct ThrottleSignalConfig {
    uint32_t debounceMs; // invalid duration before ForceZero
    uint32_t disarmMs;   // invalid duration before Disarm (must be >= debounceMs)
    uint32_t recoveryMs; // consecutive valid duration before the episode clears
};

enum class ThrottleSignalAction : uint8_t { Ok, ForceZero, Disarm };

class ThrottleSignalLogic {
public:
    ThrottleSignalLogic()
        : firstInvalidMs_(0), validRunStartMs_(0), hasInvalidEpisode_(false) {}

    ThrottleSignalAction update(bool valid, uint32_t nowMs, const ThrottleSignalConfig& cfg) {
        if (valid) {
            if (validRunStartMs_ == 0) {
                validRunStartMs_ = nowMs;
            }
            if (!hasInvalidEpisode_) {
                return ThrottleSignalAction::Ok;
            }
            if (nowMs - validRunStartMs_ >= cfg.recoveryMs) {
                hasInvalidEpisode_ = false;
                validRunStartMs_ = 0;
                return ThrottleSignalAction::Ok;
            }
            return actionForElapsed(nowMs - firstInvalidMs_, cfg);
        }

        validRunStartMs_ = 0;
        if (!hasInvalidEpisode_) {
            hasInvalidEpisode_ = true;
            firstInvalidMs_ = nowMs;
        }
        return actionForElapsed(nowMs - firstInvalidMs_, cfg);
    }

    void reset() {
        firstInvalidMs_ = 0;
        validRunStartMs_ = 0;
        hasInvalidEpisode_ = false;
    }

private:
    static ThrottleSignalAction actionForElapsed(uint32_t elapsedMs, const ThrottleSignalConfig& cfg) {
        if (elapsedMs >= cfg.disarmMs)   return ThrottleSignalAction::Disarm;
        if (elapsedMs >= cfg.debounceMs) return ThrottleSignalAction::ForceZero;
        return ThrottleSignalAction::Ok;
    }

    uint32_t firstInvalidMs_;
    uint32_t validRunStartMs_;
    bool     hasInvalidEpisode_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 test/ThrottleSignalLogicTest.cpp -o /tmp/throttle_signal_test && /tmp/throttle_signal_test`
Expected: `ThrottleSignalLogicTest: all passed`

- [ ] **Step 5: Commit**

```bash
git add src/Throttle/ThrottleSignalLogic.h test/ThrottleSignalLogicTest.cpp
git commit -m "feat: add ThrottleSignalLogic — unified wired/wireless throttle validity"
```

---

### Task 3: `ADS1115::lastReadOk()` — a real I2C health signal

**Files:**
- Modify: `src/ADS1115/ADS1115.h`
- Modify: `src/ADS1115/ADS1115.cpp`

No host test here: this class wraps `Wire`/`Adafruit_ADS1115`, both of which require the real I2C peripheral. Verification is on hardware in Task 12 (unplug the throttle Hall lead and confirm the wired disarm path fires) and by compiling both build targets.

- [ ] **Step 1: Modify `src/ADS1115/ADS1115.h`**

```cpp
#ifndef ADS1115_H
#define ADS1115_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

class ADS1115 {
    public:
        ADS1115();
        bool begin(uint8_t sdaPin, uint8_t sclPin);
        int readChannel(uint8_t channel);
        double readVoltage(uint8_t channel); // Read voltage directly (for accurate temperature calculation)
        bool isReady() { return initialized; }

        // True if the most recent readChannel() call completed a real I2C
        // transaction; false if the bus was unresponsive and a cached value
        // was returned instead. Shared across channels — reflects overall bus
        // health at the time of the last read, not a per-channel state.
        // Callers that need the health of a specific channel's read must call
        // this immediately after that channel's readChannel(), before any
        // other channel is read.
        bool lastReadOk() const { return lastReadOk_; }

    private:
        Adafruit_ADS1115 ads;
        bool initialized;
        bool lastReadOk_;
        int lastValue[4]; // Store last valid value for each channel (0-3)
        double lastVoltage[4]; // Store last valid voltage for each channel (0-3)

        // Convert 16-bit ADS1115 value (0-32767) to 12-bit equivalent (0-4095)
        int convertTo12Bit(int adsValue);

        // Cheap I2C presence probe. Wire::endTransmission() has its own
        // internal bus timeout, unlike Adafruit_ADS1X15::readADC_SingleEnded's
        // conversion-ready wait (`while (!conversionComplete());`), which has
        // none. Probing first means a wedged bus returns false here instead of
        // hanging loop() inside the library call.
        bool probeAck();

        // ADS1115 reference voltage for GAIN_ONE
        static constexpr double ADS1115_VREF = 4.096; // Volts
        static constexpr int ADS1115_MAX_VALUE = 32767; // 16-bit max value
};

#endif
```

- [ ] **Step 2: Modify `src/ADS1115/ADS1115.cpp`**

```cpp
#include "ADS1115.h"

ADS1115::ADS1115() {
    initialized = false;
    lastReadOk_ = false;
    for (int i = 0; i < 4; i++) {
        lastValue[i] = 0;
        lastVoltage[i] = 0.0;
    }
}

bool ADS1115::begin(uint8_t sdaPin, uint8_t sclPin) {
    // Configure I2C for high speed (400kHz) for faster communication
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(400000); // 400kHz I2C speed

    // Initialize ADS1115 with default address (0x48)
    if (!ads.begin()) {
        initialized = false;
        return false;
    }

    // Set gain to ±4.096V (suitable for 0-3.3V sensors)
    ads.setGain(GAIN_ONE); // ±4.096V range

    // Set data rate to 860 SPS (Samples Per Second) for fastest conversion
    // This reduces conversion time from ~8ms to ~1.2ms per reading
    ads.setDataRate(RATE_ADS1115_860SPS);

    initialized = true;

    return true;
}

bool ADS1115::probeAck() {
    Wire.beginTransmission(ADS1X15_ADDRESS);
    return Wire.endTransmission() == 0;
}

int ADS1115::readChannel(uint8_t channel) {
    if (channel > 3) {
        lastReadOk_ = false;
        return 0;  // Invalid channel — array has only 4 elements [0..3]
    }

    if (!initialized) {
        lastReadOk_ = false;
        return lastValue[channel];
    }

    if (!probeAck()) {
        // Bus is unresponsive — do not call into the library. Its
        // conversion-ready wait has no timeout and would hang loop().
        lastReadOk_ = false;
        return lastValue[channel];
    }

    int16_t rawValue = ads.readADC_SingleEnded(channel);

    // Handle error case (negative values can indicate errors in some cases)
    // But ADS1115 can return negative values for differential readings
    // For single-ended, values should be 0-32767, but let's be safe
    if (rawValue < 0) {
        // If we get a negative value in single-ended mode, use last valid value
        lastReadOk_ = false;
        return lastValue[channel];
    }

    lastReadOk_ = true;

    // Calculate voltage from raw value (for accurate calculations)
    double voltage = (rawValue * ADS1115_VREF) / ADS1115_MAX_VALUE;
    lastVoltage[channel] = voltage;

    // Convert to 12-bit equivalent (0-4095) for compatibility with existing code
    int convertedValue = convertTo12Bit(rawValue);

    // Store last valid value
    lastValue[channel] = convertedValue;

    return convertedValue;
}

double ADS1115::readVoltage(uint8_t channel) {
    if (!initialized) {
        return lastVoltage[channel];
    }

    if (channel > 3) {
        return lastVoltage[channel];
    }

    // Read channel to update voltage cache
    readChannel(channel);

    return lastVoltage[channel];
}

int ADS1115::convertTo12Bit(int adsValue) {
    // ADS1115: 16-bit (0-32767) with ±4.096V range
    // ESP32-C3 ADC: 12-bit (0-4095) with 3.3V reference
    // Convert: adcValue = (ads1115Value * 4095) / 32767
    // This maintains the same relative scale
    return (adsValue * 4095) / 32767;
}
```

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both environments build with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/ADS1115/ADS1115.h src/ADS1115/ADS1115.cpp
git commit -m "fix: report real I2C read status from ADS1115 instead of masking it"
```

---

### Task 4: `Buzzer::beepFaultDisarm()`

**Files:**
- Modify: `src/Buzzer/Buzzer.h`
- Modify: `src/Buzzer/Buzzer.cpp`

- [ ] **Step 1: Add the declaration**

In `src/Buzzer/Buzzer.h`, add next to the other contextual methods:

```cpp
  // Contextual methods
  void beepSystemStart();
  void beepCalibrationStep();
  void beepCalibrationComplete();
  void beepDisarmed();
  void beepArmingBlocked();
  void beepButtonClick();
  void beepArmedAlert();
  void beepVolumePreview();
  void beepPowerAlert();
  void beepFaultDisarm();
```

- [ ] **Step 2: Add the implementation**

In `src/Buzzer/Buzzer.cpp`, add next to `beepPowerAlert()`:

```cpp
void Buzzer::beepPowerAlert() {
  // 3 rapid beeps at 2500 Hz — distinct power-reduction warning
  startBeep(100, 3, 60, 2500);
}

void Buzzer::beepFaultDisarm() {
  // Continuous rapid double-beep at 2500 Hz — distinct from beepArmedAlert
  // (200/200 continuous at 2000 Hz) and beepPowerAlert (100/60, 3 reps,
  // 2500 Hz, not continuous). This one repeats until stop() is called on the
  // next arm/disarm transition, so it cannot be mistaken for a transient
  // warning — the system needs the pilot's attention now.
  startBeep(80, 255, 300, 2500);
}
```

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both environments build with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/Buzzer/Buzzer.h src/Buzzer/Buzzer.cpp
git commit -m "feat: add distinct fault-disarm buzzer pattern"
```

---

### Task 5: `RemoteLink::isLinkFresh()` replaces `failsafe()`

**Files:**
- Modify: `src/RemoteLink/RemoteLink.h`
- Delete: `src/RemoteLink/RemoteLinkLogic.h`
- Delete: `test/RemoteLinkLogicTest.cpp`

`RemoteLinkLogic::computeFailsafe`'s two-threshold decision (ramp at 500 ms, disarm at 3 s) moves into `ThrottleSignalLogic`'s `debounceMs`/`disarmMs` for the wireless config (Task 2, already tested). `RemoteLink` only needs to expose a raw "is a packet currently fresh" boolean — the per-tick `valid` signal `ThrottleSignalLogic` consumes.

- [ ] **Step 1: Delete the absorbed files**

```bash
git rm src/RemoteLink/RemoteLinkLogic.h test/RemoteLinkLogicTest.cpp
```

- [ ] **Step 2: Modify `src/RemoteLink/RemoteLink.h`**

```cpp
#ifndef REMOTE_LINK_H
#define REMOTE_LINK_H

#include <stdint.h>
#include "RemoteLinkProtocol.h"

class RemoteLink {
  public:
    void setup();   // init ESP-NOW on the AP channel; load paired peer from Settings
    void handle();  // periodic TX of controller state to the remote (~5 Hz)

    // Throttle / button source (used by Throttle ReadFn and the Button config).
    uint16_t lastHallRaw() const { return rx_.hallRaw; }
    bool remoteButtonPressed() const { return rx_.buttonPressed != 0; }
    uint32_t lastRxMs() const { return lastRxMs_; }
    bool hasState() const { return hasState_; }

    // Raw per-tick freshness of the throttle link: true only if we have ever
    // heard from the remote AND the last packet arrived within the last
    // kFreshWindowMs. The remote sends ~50 Hz (one packet every ~20 ms), so
    // 100 ms tolerates a handful of dropped packets — comfortably smaller
    // than ThrottleSignalLogic's 500 ms debounce, so feeding this in every
    // ~10 ms Throttle::handle() tick tracks the real packet gap closely.
    // This replaces the old computeFailsafe()/FailsafeAction split: the
    // 500 ms/3 s decision now lives in ThrottleSignalLogic's wireless config.
    bool isLinkFresh(uint32_t nowMs) const {
        if (!hasState_) return false;
        uint32_t gap = nowMs - lastRxMs_;
        // Guard against the ISR race: the ESP-NOW recv callback can update
        // the volatile lastRxMs_ between the caller's millis() snapshot and
        // this read, making lastRxMs_ > nowMs. The unsigned subtraction would
        // then underflow to a huge value; treat that as "just received",
        // not as a multi-year-old packet.
        if (gap > 0x80000000UL) return true;
        return gap < kFreshWindowMs;
    }

    // Controller -> remote state.
    void setArmed(bool armed) { tx_.armed = armed ? 1 : 0; }
    void setCalibrating(bool c) { tx_.calibrating = c ? 1 : 0; }
    void requestBeep(uint8_t beep);   // bumps the beep counter; sent on next handle()

    // Pairing.
    void enterPairing() { pairing_ = true; }
    bool isPairing() const { return pairing_; }

    // Called from the static ESP-NOW recv trampoline.
    void onReceive(const uint8_t *senderMac, const uint8_t *data, int len);

  private:
    void addPeer(const uint8_t mac[6]);
    void sendState();

    static const uint32_t kFreshWindowMs = 100;

    ThrottleToControllerPacket rx_{};
    ControllerToThrottlePacket tx_{};
    volatile bool hasState_ = false;
    volatile uint32_t lastRxMs_ = 0;
    uint8_t peerMac_[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bool pairing_ = false;
    uint32_t lastTxMs_ = 0;
};

extern RemoteLink remoteLink;

#endif // REMOTE_LINK_H
```

`RemoteLink.cpp` needs no changes — `isLinkFresh()` is header-inline and nothing else in the `.cpp` referenced `RemoteLinkLogic.h` or `failsafe()`.

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: FAIL — `main.cpp` and `config.cpp` still reference `remoteLink.failsafe()`/`FailsafeAction`. This is expected; Tasks 6-7 fix both call sites. Confirm the *only* errors are about `failsafe`/`FailsafeAction`, nothing else.

- [ ] **Step 4: Commit**

```bash
git add -u src/RemoteLink/RemoteLink.h
git commit -m "refactor: replace RemoteLink::failsafe() with a raw isLinkFresh() signal

The 500ms/3s decision moves to ThrottleSignalLogic's wireless config,
tested in test/ThrottleSignalLogicTest.cpp. RemoteLinkLogic.h and its
test are absorbed."
```

---

### Task 6: `Throttle` owns the signal-validity state machine

**Files:**
- Modify: `src/Throttle/Throttle.h`
- Modify: `src/Throttle/Throttle.cpp`

- [ ] **Step 1: Modify `src/Throttle/Throttle.h`**

```cpp
#ifndef Throttle_h
#define Throttle_h

#include "../Buzzer/Buzzer.h"
#include "../config.h"
#include "../DisarmReason.h"
#include "ThrottleEngagementLogic.h"
#include "ThrottleSignalLogic.h"

class Throttle {
    public:
        typedef int (*ReadFn)();
        typedef bool (*ReadOkFn)();
        Throttle(ReadFn readFn, ReadOkFn readOkFn);
        void handle();
        bool isArmed() { return throttleArmed; }
        void setArmed();
        void setDisarmed(DisarmReason reason = DisarmReason::Manual);
        bool isCalibrated() { return calibrated; }
        bool isEngaged() { return engagement.isEngaged(); }

        // True while an invalid throttle signal is being held at zero before
        // the disarm threshold is reached (wireless only — wired disarms
        // immediately, so it never spends time in this state).
        bool isSignalForcedZero() const { return signalForcedZero_; }
        DisarmReason getDisarmReason() const { return lastDisarmReason_; }

        unsigned int getThrottlePercentage();
        unsigned int getThrottleRaw();
        unsigned int getThrottlePinMin() { return throttlePinMin; }
        unsigned int getThrottlePinMax() { return throttlePinMax; }
        int getPinValueFiltered() { return pinValueFiltered; }
        unsigned int getCalibratingStep() { return calibratingStep; }

    private:
        const static int samples = 8;
        const unsigned int calibrationTime = 3000; // 3 seconds for calibration
        const int calibrationThreshold = 2000; // Threshold for detecting throttle movement

        int pinValues[samples];
        int pinValueFiltered;
        ThrottleEngagementLogic engagement;
        unsigned long lastThrottleRead;

        volatile bool throttleArmed;

        bool calibrated;
        unsigned int calibratingStep;
        unsigned long calibrationStartTime;
        int calibrationMaxValue;
        int calibrationMinValue;
        unsigned long calibrationSumMax;
        unsigned int calibrationCountMax;
        unsigned long calibrationSumMin;
        unsigned int calibrationCountMin;

        unsigned int armingTries;

        int throttlePinMin;
        int throttlePinMax;

        ReadFn readFn;
        ReadOkFn readOkFn;
        bool lastSampleOk_;

        ThrottleSignalLogic signalLogic;
        bool signalForcedZero_;
        DisarmReason lastDisarmReason_;

        void readThrottlePin();
        void resetCalibration();
        void handleCalibration(unsigned long now);
        void updateSignalValidity(unsigned long now);
};

#endif
```

- [ ] **Step 2: Modify `src/Throttle/Throttle.cpp`**

```cpp
#include <Arduino.h>
#include "Throttle.h"

#include "../config.h"

Throttle::Throttle(ReadFn readFn, ReadOkFn readOkFn) : readFn(readFn), readOkFn(readOkFn) {
  memset(
    &pinValues,
    0,
    sizeof(pinValues[0]) * samples
  );

  pinValueFiltered = 0;
  lastThrottleRead = 0;

  throttleArmed = false;
  lastSampleOk_ = true;
  signalForcedZero_ = false;
  lastDisarmReason_ = DisarmReason::None;
  resetCalibration();

  throttlePinMin = 0;
  throttlePinMax = 0;

  armingTries = 0;
}

void Throttle::handle()
{
  unsigned long now = millis();

  if (now - lastThrottleRead < 10) {
    return;
  }

  lastThrottleRead = now;
  readThrottlePin();
  engagement.update(pinValueFiltered, throttlePinMin, throttlePinMax);

  // Handle calibration if not yet calibrated
  if (!calibrated) {
    handleCalibration(now);
    return; // band isn't established yet — signal-validity checks need it
  }

  updateSignalValidity(now);
}

void Throttle::updateSignalValidity(unsigned long now)
{
  bool wireless = settings.getThrottleSource() == ThrottleSourceWireless;
  bool valid;

  if (wireless) {
    // Wireless validity is the raw link-freshness signal; the debounce/
    // disarm tolerance lives entirely in the wireless ThrottleSignalConfig
    // below, not in what counts as "valid" here.
    valid = lastSampleOk_;
  } else {
    // Wired validity: the filtered (8-sample averaged) reading must fall
    // within the calibrated band, with an asymmetric margin — reading low
    // only costs power, reading high is the side that becomes unintended
    // acceleration, so it gets a tighter margin. The moving average already
    // absorbs a single spurious sample (see ThrottleSignalLogic's header
    // comment), so debounceMs=0 below is still safe against normal ADC noise.
    int range = throttlePinMax - throttlePinMin;
    int marginLow  = (range * 20) / 100;
    int marginHigh = (range * 10) / 100;
    bool inBand = pinValueFiltered >= (throttlePinMin - marginLow) &&
                  pinValueFiltered <= (throttlePinMax + marginHigh);
    valid = lastSampleOk_ && inBand;
  }

  ThrottleSignalConfig cfg = wireless
      ? ThrottleSignalConfig{500, 3000, 200}
      : ThrottleSignalConfig{0, 0, 0};

  ThrottleSignalAction action = signalLogic.update(valid, now, cfg);

  switch (action) {
    case ThrottleSignalAction::Ok:
      signalForcedZero_ = false;
      break;
    case ThrottleSignalAction::ForceZero:
      signalForcedZero_ = true;
      break;
    case ThrottleSignalAction::Disarm:
      signalForcedZero_ = true;
      setDisarmed(wireless ? DisarmReason::ThrottleLinkLost : DisarmReason::ThrottleWiredInvalid);
      break;
  }
}

void Throttle::resetCalibration()
{
  calibrated = false;
  calibratingStep = 0;
  calibrationStartTime = 0;
  calibrationMaxValue = 0;
  calibrationMinValue = ADC_MAX_VALUE; // Start with max possible value for ADC (4095 for 12-bit)
  calibrationSumMax = 0;
  calibrationCountMax = 0;
  calibrationSumMin = 0;
  calibrationCountMin = 0;
  engagement.reset();
  signalLogic.reset();
  signalForcedZero_ = false;
}

void Throttle::handleCalibration(unsigned long now)
{
  // Step 0: Calibrate maximum throttle
  if (calibratingStep == 0) {
    // Check if throttle is above threshold
    if (pinValueFiltered > calibrationThreshold) {
      // Start timing if not already started
      if (calibrationStartTime == 0) {
        calibrationStartTime = now;
        calibrationSumMax = 0;
        calibrationCountMax = 0;
      }

      // Accumulate values for averaging
      calibrationSumMax += pinValueFiltered;
      calibrationCountMax++;

      // Track the maximum value seen (kept for compatibility, but not used anymore)
      if (pinValueFiltered > calibrationMaxValue) {
        calibrationMaxValue = pinValueFiltered;
      }

      // Check if we've held the throttle for the required time
      if (now - calibrationStartTime >= calibrationTime && calibrationCountMax > 0) {
        // Set the max throttle value as the average
        throttlePinMax = calibrationSumMax / calibrationCountMax;

        buzzer.beepCalibrationStep();

        // Move to next step
        calibratingStep = 1;
        calibrationStartTime = 0; // Reset timer for next step
      }
      return;
    }

    // Reset timer and accumulators if throttle drops below threshold
    calibrationStartTime = 0;
    calibrationSumMax = 0;
    calibrationCountMax = 0;
    return;
  }

  // Step 1: Calibrate minimum throttle
  if (calibratingStep == 1) {
    // Check if throttle is below threshold
    if (pinValueFiltered < calibrationThreshold) {
      // Start timing if not already started
      if (calibrationStartTime == 0) {
        calibrationStartTime = now;
        calibrationSumMin = 0;
        calibrationCountMin = 0;
      }

      // Accumulate values for averaging
      calibrationSumMin += pinValueFiltered;
      calibrationCountMin++;

      // Track the minimum value seen (kept for compatibility, but not used anymore)
      if (pinValueFiltered < calibrationMinValue) {
        calibrationMinValue = pinValueFiltered;
      }

      // Check if we've held the throttle for the required time
      if (now - calibrationStartTime >= calibrationTime && calibrationCountMin > 0) {
        // Set the min throttle value as the average
        throttlePinMin = calibrationSumMin / calibrationCountMin;

        // Calibration complete
        calibrated = true;
        buzzer.beepCalibrationComplete();
        return;
      }

      return;
    }

    // Reset timer and accumulators if throttle goes above threshold
    calibrationStartTime = 0;
    calibrationSumMin = 0;
    calibrationCountMin = 0;
    return;
  }
}

void Throttle::readThrottlePin()
{
  memmove(
    &pinValues,
    &pinValues[1],
    sizeof(pinValues[0]) * (samples - 1)
  );

  int oversampledValue = readFn();
  pinValues[samples - 1] = oversampledValue;
  // Must be read immediately after readFn(), before any other ADS1115
  // channel is read elsewhere in loop() — lastReadOk() reflects whichever
  // channel was read most recently.
  lastSampleOk_ = readOkFn();

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  pinValueFiltered = sum / samples;
}

unsigned int Throttle::getThrottlePercentage()
{
  // Check if throttle is calibrated (min and max are different)
  if (!calibrated || throttlePinMin == throttlePinMax) {
    return 0;
  }

  int pinValueConstrained = getThrottleRaw();
  unsigned int throttlePercentage = map(pinValueConstrained, throttlePinMin, throttlePinMax, 0, 100);

  if (throttlePercentage < 5) {
    return 0;
  }

  if (throttlePercentage > 95) {
    return 100;
  }

  return throttlePercentage;
}

unsigned int Throttle::getThrottleRaw()
{
  int pinValueConstrained = constrain(pinValueFiltered, throttlePinMin, throttlePinMax);
  return pinValueConstrained;
}

void Throttle::setArmed()
{
  power.resetBatteryPowerFloor();
  if (throttleArmed) {
    return;
  }

  // Don't allow arming if not calibrated
  if (!calibrated) {
    buzzer.beepArmingBlocked();  // Warning: must calibrate throttle first
    return;
  }

  if (getThrottlePercentage() > 0) {
    buzzer.beepArmingBlocked();
    armingTries++;

    if (armingTries > 2) {
      armingTries = 0;
      resetCalibration();
    }

    return;
  }

  signalLogic.reset();
  signalForcedZero_ = false;
  lastDisarmReason_ = DisarmReason::None;
  throttleArmed = true;
}

void Throttle::setDisarmed(DisarmReason reason)
{
  if (!throttleArmed) {
    return;
  }

  throttleArmed = false;
  lastDisarmReason_ = reason;

  if (reason == DisarmReason::Manual) {
    buzzer.beepDisarmed();
  } else {
    buzzer.beepFaultDisarm();
  }
}
```

Note on `getThrottleRaw()`/`getThrottlePercentage()`: **unchanged**, deliberately. Wired never reaches `ForceZero` (it disarms immediately instead), and wireless's `ForceZero` state is realized by the `ReadFn` in `config.cpp` (Task 7) feeding 0 into the moving average — the same mechanism as today's ramp-to-zero, just gated by `Throttle::isSignalForcedZero()` instead of `remoteLink.failsafe()`. This preserves the existing gradual ramp-down UX for link loss instead of an abrupt cut.

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: FAIL — `Throttle throttle(...)` in `config.cpp` still uses the single-argument constructor. Confirm the only error is that call site; fixed in Task 7.

- [ ] **Step 4: Commit**

```bash
git add src/Throttle/Throttle.h src/Throttle/Throttle.cpp
git commit -m "feat: Throttle disarms on invalid signal (wired: immediately, wireless: existing timing)"
```

---

### Task 7: Wire up `config.cpp` and clean up `main.cpp`

**Files:**
- Modify: `src/config.cpp`
- Modify: `src/main.cpp`

- [ ] **Step 1: Modify the `Throttle` instantiation in `src/config.cpp`**

Replace:

```cpp
Throttle throttle([]() -> int {
    if (settings.getThrottleSource() == ThrottleSourceWireless) {
        // Failsafe ramp: feed 0 so the existing ramp-down drives the motor to idle.
        if (remoteLink.failsafe(true, millis()) != FailsafeAction::None) return 0;
        return (int)remoteLink.lastHallRaw();
    }
    return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL);
});
```

with:

```cpp
Throttle throttle(
    []() -> int {
        if (settings.getThrottleSource() == ThrottleSourceWireless) {
            // Ramp to zero while the signal is invalid but not yet disarmed
            // (ThrottleSignalLogic's ForceZero state) — feeds the existing
            // moving average so the motor winds down smoothly instead of
            // cutting abruptly.
            if (throttle.isSignalForcedZero()) return 0;
            return (int)remoteLink.lastHallRaw();
        }
        return ads1115.readChannel(ADS1115_THROTTLE_CHANNEL);
    },
    []() -> bool {
        if (settings.getThrottleSource() == ThrottleSourceWireless) {
            return remoteLink.isLinkFresh(millis());
        }
        return ads1115.lastReadOk();
    }
);
```

This compiles even though the lambda refers to the global `throttle` it is initializing: neither lambda captures anything (both convert to plain function pointers), and they are only *called* later from `Throttle::handle()` in `loop()`, long after `throttle`'s constructor has finished running. `config.h` (included above this point) already declares `extern Throttle throttle;`.

- [ ] **Step 2: Remove the absorbed wireless failsafe block from `src/main.cpp`**

Delete these lines from `loop()` (currently right after `remoteLink.handle();`):

```cpp
  // Wireless failsafe: prolonged link loss disarms (the ramp-to-zero case is
  // handled in the throttle ReadFn feeding 0). See RemoteLinkLogic.
  if (settings.getThrottleSource() == ThrottleSourceWireless &&
      throttle.isArmed() &&
      remoteLink.failsafe(true, millis()) == FailsafeAction::Disarm) {
    throttle.setDisarmed();
  }

```

`Throttle::handle()` (called a few lines earlier in the same `loop()` iteration, at `throttle.handle();`) now performs this disarm itself via `updateSignalValidity()`, for both wired and wireless.

- [ ] **Step 3: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both environments build with no errors.

- [ ] **Step 4: Run the full host test suite**

```bash
c++ -std=c++17 test/DisarmReasonTest.cpp -o /tmp/disarm_reason_test && /tmp/disarm_reason_test
c++ -std=c++17 test/ThrottleSignalLogicTest.cpp -o /tmp/throttle_signal_test && /tmp/throttle_signal_test
c++ -std=c++17 test/ThrottleEngagementLogicTest.cpp -o /tmp/engagement_test && /tmp/engagement_test
c++ -std=c++17 test/PowerAlertLogicTest.cpp -o /tmp/power_alert_test && /tmp/power_alert_test
c++ -std=c++17 test/PowerTest.cpp -o /tmp/power_test && /tmp/power_test
c++ -std=c++17 test/JkBmsParserTest.cpp -o /tmp/jk_test && /tmp/jk_test
```

Expected: every binary prints its `all passed` / `PASS` line. `RemoteLinkProtocolTest.cpp` is untouched by this change and does not need re-running here, but running it costs nothing extra: `c++ -std=c++17 test/RemoteLinkProtocolTest.cpp -o /tmp/protocol_test && /tmp/protocol_test`.

- [ ] **Step 5: Commit**

```bash
git add src/config.cpp src/main.cpp
git commit -m "refactor: unify wired/wireless throttle disarm through Throttle itself

main.cpp no longer special-cases wireless failsafe disarm — Throttle::handle()
now owns both paths via ThrottleSignalLogic."
```

---

### Task 8: Surface `disarmReason` on `/api/telemetry`

**Files:**
- Modify: `src/WebServer/ControllerWebServer.cpp`

- [ ] **Step 1: Add the include and the field**

Add near the other relative includes at the top of `src/WebServer/ControllerWebServer.cpp`:

```cpp
#include "../DisarmReason.h"
```

In the `/api/telemetry` handler, right after the existing `doc["armed"] = throttle.isArmed();` line:

```cpp
        doc["armed"] = throttle.isArmed();
        doc["disarmReason"] = disarmReasonCode(throttle.getDisarmReason());
```

- [ ] **Step 2: Compile both build targets**

Run: `~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag`
Expected: both environments build with no errors.

- [ ] **Step 3: Commit**

```bash
git add src/WebServer/ControllerWebServer.cpp
git commit -m "feat: expose disarmReason on /api/telemetry"
```

---

### Task 9: Fault-disarm warning panel on the telemetry page

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h`

Reuses the existing `.power-alert-panel`/`.power-alert-title`/`.power-alert-causes` CSS classes (already in `src/WebServer/Pages/CommonStyles.h`) — no CSS changes needed. Unlike the power-alert panel, this one has no close button: it is not dismissible, since it is the record the pilot needs to diagnose what happened.

- [ ] **Step 1: Add the panel markup**

In `src/WebServer/Pages/TelemetryPage.h`, right after the existing `power-alert-panel` div:

```html
            <div class="power-alert-panel" id="powerAlertPanel">
                <div style="flex:1;">
                    <div class="power-alert-title">&#x26A0; Pot&#xEA;ncia reduzida</div>
                    <div class="power-alert-causes" id="powerAlertCauses"></div>
                </div>
                <button type="button" class="power-alert-close" id="powerAlertClose" aria-label="Fechar alerta">&#x2715;</button>
            </div>

            <div class="power-alert-panel" id="faultDisarmPanel">
                <div style="flex:1;">
                    <div class="power-alert-title">&#x26A0; Desarmado por falha no sinal do acelerador</div>
                    <div class="power-alert-causes" id="faultDisarmReason"></div>
                </div>
            </div>
```

- [ ] **Step 2: Add the render function and wire it into the poll loop**

In the JS section, right after `renderPowerAlert`'s definition (before `const initPowerAlert = ...`):

```js
// ============ Fault Disarm ============
const FAULT_DISARM_LABELS = {
    'THR ERR':  'Acelerador com fio: leitura fora da faixa calibrada ou falha de leitura do ADS1115.',
    'LINK ERR': 'Acelerador sem fio: link com o remote perdido por mais de 3 segundos.',
};

const renderFaultDisarm = (data) => {
    const panel = $('faultDisarmPanel');
    const reasonEl = $('faultDisarmReason');
    if (!panel || !reasonEl) return;

    const reason = data.disarmReason || '';
    const isFault = !data.armed && reason !== '' && reason !== 'MANUAL';

    if (isFault) {
        reasonEl.textContent = FAULT_DISARM_LABELS[reason] || `Código: ${reason}`;
        panel.classList.add('open');
    } else {
        panel.classList.remove('open');
    }
};
```

Then, in `renderTelemetry`, right after the existing `renderPowerAlert(data.powerAlert);` line:

```js
    renderPowerAlert(data.powerAlert);
    renderFaultDisarm(data);
};
```

- [ ] **Step 3: Manual verification**

Start the web server on a bench unit (or via `pio device monitor` after flashing), open `/telemetry`, and:
1. Confirm the page loads with the panel hidden (no `disarmReason` yet, or `armed: true`).
2. Simulate a wired throttle fault (disconnect the Hall sensor lead while armed) and confirm the panel appears with the wired message and stays open (no close button) until the system is re-armed.
3. Re-arm and confirm the panel disappears.

- [ ] **Step 4: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: show a persistent fault-disarm warning on the telemetry page"
```

---

### Task 10: Documentation

**Files:**
- Modify: `CLAUDE.md`
- Modify: `src/CLAUDE.md`
- Modify: `docs/MANUAL-DE-USO.md`
- Modify: `docs/MANUAL-INTERFACE-WEB.md`

- [ ] **Step 1: Update `CLAUDE.md`**

Replace this line (in the "Wireless Throttle (ESP-NOW)" section):

```markdown
- **Failsafe (`RemoteLink/RemoteLinkLogic.h`, host-tested):** wireless mode only. No packet for >500 ms → ramp throttle to 0 (stay armed, via the ReadFn feeding 0); >3 s → disarm.
```

with:

```markdown
- **Signal validity (`Throttle/ThrottleSignalLogic.h`, host-tested):** a single state machine handles both throttle sources. Wired: any out-of-calibrated-band reading or ADS1115 I2C failure disarms immediately, no tolerance. Wireless: no packet for >500 ms → ramp throttle to 0 (stay armed, via the ReadFn feeding 0); >3 s → disarm. Either path sets a latched `DisarmReason` (`src/DisarmReason.h`), shown on the telemetry page until the next successful arm.
```

- [ ] **Step 2: Update `src/CLAUDE.md`**

In the "### Throttle — `Throttle/`" entry, add a sentence after the existing `ThrottleEngagementLogic` sentence:

```markdown
### Throttle — `Throttle/`
Hall sensor input. Accepts `ReadFn`. Handles arming state machine, calibration (3-second sweep), and an 8-sample moving average. `throttle.isArmed()` gates ESC output. A separate engage/release hysteresis gate (`ThrottleEngagementLogic`, host-tested in `test/ThrottleEngagementLogicTest.cpp`) protects against Hall-sensor drift/noise at idle — `throttle.isEngaged()` must be true (filtered reading > `throttlePinMin + 2%` of range, released below `+1%`) before `Power` will output anything above `ESC_MIN_PWM`. Signal validity (`ThrottleSignalLogic`, host-tested in `test/ThrottleSignalLogicTest.cpp`, also takes a `ReadOkFn` alongside `ReadFn`) disarms on an invalid reading — immediately for the wired source, after the existing 500ms/3s tolerance for the wireless source — and records why in a latched `DisarmReason` (`src/DisarmReason.h`).
```

- [ ] **Step 3: Update `docs/MANUAL-DE-USO.md`**

Add a new subsection after section 3 ("Armar e desarmar"), renumbering the following sections (`4` → `5`, etc. — adjust every subsequent `##` heading number in the file by one):

```markdown
## 4. Desarme automático por falha no acelerador

O sistema desarma automaticamente, sem intervenção do piloto, se a leitura do acelerador deixar de ser confiável:

- **Acelerador com fio:** qualquer leitura fora da faixa calibrada (por exemplo, cabo do sensor Hall rompido ou solto) ou falha de comunicação com o ADS1115 desarma **imediatamente**.
- **Acelerador sem fio (remote):** perda de sinal do remote por mais de 3 segundos desarma o sistema. Entre 500 ms e 3 s sem sinal, a potência é reduzida gradualmente a zero, mas o sistema continua armado.

Em qualquer um dos dois casos:
- O buzzer toca um padrão de alarme contínuo, diferente do beep de desarme manual.
- A página de Telemetria mostra um aviso permanente na tela informando o motivo (não desaparece sozinho).
- Para voltar a voar, primeiro corrija o problema (verifique a conexão do sensor/cabo, ou o link do remote) e depois arme novamente pelo procedimento normal.
```

- [ ] **Step 4: Update `docs/MANUAL-INTERFACE-WEB.md`**

In section 9 ("Observações técnicas"), extend the existing `/api/telemetry` paragraph:

```markdown
- A API de telemetria está em **GET /api/telemetry** (JSON). O objeto **availability** indica quais dados estão disponíveis (`current`, `rpm`, `powerKw`, `bms`, `bmsCells`). Campos numéricos como `rpm`, `escCurrentMa` e `powerKwX10` são omitidos quando indisponíveis (a página mostra N/A). O campo **disarmReason** indica o motivo do último desarme: vazio (nunca desarmou desde o boot), `MANUAL` (desarme normal pelo botão/interface), ou um código de falha (`THR ERR` = acelerador com fio inválido, `LINK ERR` = link do remote perdido) — a página de Telemetria mostra um aviso permanente enquanto o código de falha estiver ativo e o sistema estiver desarmado. Quando o BMS está conectado, o objeto **bms** traz `tempMaxC`, `cellMinMv`, `cellMaxMv` e `cellDeltaMv`. O campo **buzzer** é um array com os últimos eventos de beep (até 8, do mais antigo ao mais recente): cada entrada tem `seq` (contador monotônico), `freq` (Hz), `onMs`, `offMs`, `reps` (255 = contínuo) e `active` (true = iniciado, false = parado). A página de Telemetria usa esses dados para reproduzir os beeps no navegador via Web Audio API. A página de Configuração usa **GET /config/values** (ler) e **POST /config/save** (gravar) com corpo JSON.
```

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md src/CLAUDE.md docs/MANUAL-DE-USO.md docs/MANUAL-INTERFACE-WEB.md
git commit -m "docs: document throttle signal-validity disarm behavior"
```

---

### Task 11: Final verification

**Files:** none (verification only)

- [ ] **Step 1: Full host test suite**

```bash
for f in DisarmReasonTest ThrottleSignalLogicTest ThrottleEngagementLogicTest PowerAlertLogicTest PowerTest JkBmsParserTest RemoteLinkProtocolTest; do
  c++ -std=c++17 test/$f.cpp -o /tmp/$f && /tmp/$f || echo "FAILED: $f";
done
```

Expected: every test prints its pass line, no `FAILED` lines. (`RemoteLinkLogicTest` is gone — deleted in Task 5.)

- [ ] **Step 2: Full firmware build, both targets**

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor -e lolin_c3_mini_xag
```

Expected: `SUCCESS` for both environments.

- [ ] **Step 3: Bench verification (wired build)**

Flash `lolin_c3_mini_tmotor` (or `_xag`) to a bench unit with the throttle source set to wired (default). With the throttle calibrated and the system armed:

1. Disconnect the Hall sensor signal wire (or short it to a rail so the reading goes out of the calibrated band). Confirm: the motor cuts to `ESC_MIN_PWM` immediately, the fault-disarm buzzer pattern plays, and `/telemetry` shows the persistent warning with `THR ERR`.
2. Reconnect the wire, re-arm via the normal button gesture. Confirm the system arms normally and the warning clears.

- [ ] **Step 4: Bench verification (wireless build)**

Set the throttle source to wireless, pair a remote, arm the system.

1. Power off the remote. Confirm: between ~500 ms and ~3 s the motor ramps down to zero while staying armed (no beep yet, matches today's existing behavior); at 3 s the system disarms, the fault-disarm buzzer plays, and `/telemetry` shows `LINK ERR`.
2. Power the remote back on, re-arm. Confirm the system arms normally.

- [ ] **Step 5: Report results to the user**

Summarize pass/fail for each of the steps above before considering this plan complete.
