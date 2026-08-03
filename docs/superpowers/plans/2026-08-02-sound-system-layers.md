# Sound System Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **WORKING DOCUMENT — REMOVE BEFORE OPENING THE PR**, together with
> `docs/superpowers/specs/2026-08-02-sound-system-design.md`. Both are gitignored and
> committed with `git add -f` so they travel with the branch during implementation.

**Goal:** Replace the single-slot `Buzzer` (where any new beep permanently kills the
continuous armed alert, and where the alert itself silently expires after ~102 s) with a
layered audio system: a driver-only `Buzzer`, a pure host-tested `SoundLogic` policy
(event queue + persistent state layer), and a thin `Sound` component gluing them
together.

**Architecture:** `Buzzer` shrinks to `toneOn/toneOff/setVolume/recalibrate` — pure LEDC
hardware. `Sound/SoundLogic.h` is pure C++ (no Arduino deps, host-testable like
`PowerAlertLogic.h`/`RemoteLinkLogic.h`): a 4-slot event queue plus one `SoundState`
value, recalculated every tick so a preempted continuous sound always resumes on its own.
`Sound/Sound.{h,cpp}` owns the frequency/timing catalog, drives `SoundLogic` each loop
tick, and translates its output into `Buzzer` calls plus a `BeepEvent` ring for
`/api/telemetry`.

**Tech Stack:** ESP32-C3 Arduino/PlatformIO (`~/.platformio/penv/bin/pio`), C++17 host
tests compiled directly with `c++ -std=c++17` (no PlatformIO test runner — see
`test/CLAUDE.md`).

---

## Full spec

Read `docs/superpowers/specs/2026-08-02-sound-system-design.md` first — this plan
implements it exactly. Key facts repeated here so each task is self-contained:

- **Two enums, not one.** `SoundEvent` (queue-only, momentary) and `SoundState`
  (persistent, exactly one active). `play(SoundEvent)` cannot accept a state id and vice
  versa — this makes the original "armed alert never comes back" bug a compile-time
  impossibility, not a runtime one.
- **`reps == 0` means genuinely continuous.** No counter that can expire. This is the fix
  for the second bug (armed alert self-terminating after 255 reps × 400 ms = 102 s).
- **`isMotorRunning()` switches from `getThrottlePercentage() > 1` to
  `throttle.isEngaged()`** — the existing 2%/1% hysteresis gate. This removes the Hall
  noise that was re-triggering the armed alert and lets the remote-beep channel track the
  same "armed + stopped" condition as the controller (no more decoupled rules).
- **New: an audible link-loss warning** during the wireless failsafe ramp window (500 ms
  – 3 s), where today there is silence.
- **Browser mirror simplification:** the `/api/telemetry` `buzzer` array gains a `layer`
  field (0 = event, 1 = state); `reps: 255` stops meaning "infinite" on either side.

---

## File Structure

**New:**
- `src/Sound/PeriodicTrigger.h` — pure periodic-fire timing (used by both the power alert
  and the new link-loss warning)
- `src/Sound/SoundLogic.h` — pure queue + state resolution policy, plus the
  `computeToneTransition` hardware-transition helper
- `src/Sound/Sound.h` / `src/Sound/Sound.cpp` — the component: catalog, ring buffer,
  `Buzzer` glue
- `test/PeriodicTriggerTest.cpp`
- `test/SoundLogicTest.cpp`

**Modified:**
- `src/Buzzer/Buzzer.h` / `.cpp` — shrinks to hardware driver only
- `src/PowerAlert/PowerAlertLogic.h` — becomes a thin wrapper over `PeriodicTrigger`
- `src/PowerAlert/PowerAlert.cpp` — calls `sound.play()` instead of `buzzer.beepPowerAlert()`
- `src/config.h` / `src/config.cpp` — new `sound` global, new `SOUND_LINK_LOSS_INTERVAL_MS`
- `src/main.h` / `src/main.cpp` — loop order, `updateSoundState()`, `isMotorRunning()`,
  link-loss trigger, setup calls
- `src/Button/Button.cpp` — `sound.play(SoundEvent::ButtonClick)`
- `src/Throttle/Throttle.h` / `.cpp` — beep renames, drops the unused `Buzzer.h` include
- `src/WebServer/ControllerWebServer.cpp` — `Sound::kRingSize`, `layer` field
- `src/WebServer/Pages/TelemetryPage.h` — layered browser mirror
- `src/CLAUDE.md`, `CLAUDE.md`, `test/CLAUDE.md` — docs

---

### Task 1: `PeriodicTrigger` — pure periodic-fire timing

**Files:**
- Create: `src/Sound/PeriodicTrigger.h`
- Test: `test/PeriodicTriggerTest.cpp`

- [ ] **Step 1: Write the failing test**

Create `test/PeriodicTriggerTest.cpp`:

```cpp
#include <iostream>
#include <cassert>
#include <stdint.h>
#include <stdbool.h>
using namespace std;

#include "../src/Sound/PeriodicTrigger.h"

void test_fires_on_entry_and_every_interval() {
    PeriodicTrigger trig;
    assert(trig.update(false, 0, 1000) == false);
    assert(trig.update(true, 1000, 1000) == true);   // entry -> fires
    assert(trig.update(true, 1500, 1000) == false);  // < interval since last fire
    assert(trig.update(true, 2000, 1000) == true);   // interval elapsed -> fires
    assert(trig.update(true, 2999, 1000) == false);
    assert(trig.update(true, 3000, 1000) == true);
    cout << "PASS: fires on entry and every interval while active\n";
}

void test_resets_when_condition_clears() {
    PeriodicTrigger trig;
    assert(trig.update(true, 0, 1000) == true);
    assert(trig.update(false, 500, 1000) == false);  // cleared before the interval
    assert(trig.update(true, 600, 1000) == true);    // re-entry fires immediately
    cout << "PASS: resets when the condition clears\n";
}

void test_survives_millis_rollover() {
    PeriodicTrigger trig;
    uint32_t nearMax = 0xFFFFFFF0u; // 16ms before wraparound
    assert(trig.update(true, nearMax, 1000) == true); // entry

    uint32_t justAfterWrap = 500u;  // real elapsed since fire: 16 + 500 = 516ms
    assert(trig.update(true, justAfterWrap, 1000) == false); // 516 < 1000

    uint32_t pastInterval = 1000u;  // real elapsed: 16 + 1000 = 1016ms >= 1000
    assert(trig.update(true, pastInterval, 1000) == true);
    cout << "PASS: survives millis() rollover\n";
}

int main() {
    test_fires_on_entry_and_every_interval();
    test_resets_when_condition_clears();
    test_survives_millis_rollover();
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 -o /tmp/periodic_test test/PeriodicTriggerTest.cpp`
Expected: FAIL to compile — `../src/Sound/PeriodicTrigger.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Create `src/Sound/PeriodicTrigger.h`:

```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Fires immediately when `active` becomes true, then again every intervalMs
// while it stays true. Resets when `active` goes false.
//
// Time comparisons use unsigned subtraction (nowMs - lastFireMs_), which
// yields the correct elapsed duration even when millis() wraps past
// UINT32_MAX (~49 days uptime) -- unsigned wraparound arithmetic is exact
// as long as the real elapsed time fits in 32 bits.
class PeriodicTrigger {
public:
    PeriodicTrigger() : wasActive_(false), lastFireMs_(0) {}

    bool update(bool active, uint32_t nowMs, uint32_t intervalMs) {
        if (!active) {
            wasActive_ = false;
            lastFireMs_ = 0;
            return false;
        }

        if (!wasActive_) {
            wasActive_ = true;
            lastFireMs_ = nowMs;
            return true;
        }

        if (nowMs - lastFireMs_ >= intervalMs) {
            lastFireMs_ = nowMs;
            return true;
        }

        return false;
    }

private:
    bool     wasActive_;
    uint32_t lastFireMs_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 -o /tmp/periodic_test test/PeriodicTriggerTest.cpp && /tmp/periodic_test`
Expected:
```
PASS: fires on entry and every interval while active
PASS: resets when the condition clears
PASS: survives millis() rollover
```

- [ ] **Step 5: Commit**

```bash
git add src/Sound/PeriodicTrigger.h test/PeriodicTriggerTest.cpp
git commit -m "feat: add PeriodicTrigger pure timing helper"
```

---

### Task 2: `PowerAlertLogic` becomes a `PeriodicTrigger` wrapper

**Files:**
- Modify: `src/PowerAlert/PowerAlertLogic.h`
- Test (unchanged, must still pass): `test/PowerAlertLogicTest.cpp`

- [ ] **Step 1: Run the existing test to confirm the baseline passes**

Run: `c++ -std=c++17 -o /tmp/power_alert_test test/PowerAlertLogicTest.cpp && /tmp/power_alert_test`
Expected: all `PASS:` lines, exit 0 (this is the pre-refactor baseline).

- [ ] **Step 2: Replace the implementation**

Replace the full contents of `src/PowerAlert/PowerAlertLogic.h`:

```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../Sound/PeriodicTrigger.h"

// Pure power-alert timing logic -- no Arduino deps, host-testable.
// Call update() each loop tick; returns true when a beep should be emitted.
//
// Fires immediately on the transition into the limited state, then once every
// intervalMs while it persists. Resets when limited is no longer true.
//
// A thin wrapper over the general-purpose PeriodicTrigger (see Sound/), which
// the wireless link-loss warning also uses.
class PowerAlertLogic {
public:
    bool update(bool armed, uint8_t activeCauses, uint32_t nowMs, uint32_t intervalMs) {
        bool limited = armed && (activeCauses != 0);
        return trigger_.update(limited, nowMs, intervalMs);
    }

private:
    PeriodicTrigger trigger_;
};
```

- [ ] **Step 3: Run the existing test to verify it still passes, unchanged**

Run: `c++ -std=c++17 -o /tmp/power_alert_test test/PowerAlertLogicTest.cpp && /tmp/power_alert_test`
Expected: identical output to Step 1 — the public API (`PowerAlertLogic logic;` then
`logic.update(armed, causes, nowMs, intervalMs)`) is unchanged, so the test file itself
needs no edits.

- [ ] **Step 4: Commit**

```bash
git add src/PowerAlert/PowerAlertLogic.h
git commit -m "refactor: rebuild PowerAlertLogic on top of PeriodicTrigger"
```

---

### Task 3: `SoundLogic` — the layered queue + state policy

This is the core of the redesign — the class that makes both original bugs impossible by
construction. Read every test carefully; each one maps directly to a bug or a design rule
in the spec.

**Files:**
- Create: `src/Sound/SoundLogic.h`
- Test: `test/SoundLogicTest.cpp`

- [ ] **Step 1: Write the failing test**

Create `test/SoundLogicTest.cpp`:

```cpp
#include <iostream>
#include <cassert>
#include <stdint.h>
#include <stdbool.h>
using namespace std;

#include "../src/Sound/SoundLogic.h"

void test_continuous_state_never_expires() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});
    logic.setState(SoundState::ArmedIdle);

    bool expectedOn = true;
    for (int i = 0; i < 2000; i++) {
        // 2000 half-cycles * 200ms = 400,000ms (~6.6 min) simulated, far past
        // the old 255-rep / ~102s cutoff that used to silence this alert.
        uint32_t t = (uint32_t)i * 200;
        SoundOutput o = logic.update(t);
        assert(o.toneOn == expectedOn);
        assert(o.freqHz == 2000);
        expectedOn = !expectedOn;
    }
    cout << "PASS: continuous state alternates for 2000 half-cycles, never expires\n";
}

void test_event_preempts_state_and_resumes_from_start() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});
    logic.setEventPattern(SoundEvent::ButtonClick, {3000, 50, 0, 1});

    logic.setState(SoundState::ArmedIdle);
    SoundOutput o = logic.update(0);
    assert(o.toneOn && o.freqHz == 2000);

    // Halfway through the state's on-phase, an event preempts it immediately.
    logic.pushEvent(SoundEvent::ButtonClick);
    o = logic.update(50);
    assert(o.toneOn && o.freqHz == 3000);
    assert(!logic.stateActive()); // cut, not paused-in-place

    o = logic.update(90); // elapsed 40 < 50(onMs), still playing
    assert(o.toneOn && o.freqHz == 3000);

    o = logic.update(100); // elapsed 50 >= 50(onMs), reps=1 -> finishes, hands off to state
    assert(o.toneOn && o.freqHz == 2000);
    assert(logic.stateActive());

    // "Resumes from the start of its cycle": a fresh 200ms on-phase from
    // t=100, not a continuation of the original cycle (which started at t=0
    // and would already be in its off-phase by t=299). Pick timings so the
    // two hypotheses disagree.
    o = logic.update(100 + 199);
    assert(o.toneOn); // still on 199ms after the resume point
    o = logic.update(100 + 200);
    assert(!o.toneOn); // flips off exactly 200ms after the RESUME point (t=300)

    cout << "PASS: event preempts state immediately and state resumes from cycle start\n";
}

void test_setstate_idempotent_and_none_silences_immediately() {
    SoundLogic logic;
    logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});

    logic.setState(SoundState::ArmedIdle);
    assert(logic.update(0).toneOn == true);

    // Calling setState() with the same value every tick must not restart
    // the on-phase.
    for (uint32_t t = 1; t <= 199; t++) {
        logic.setState(SoundState::ArmedIdle);
        assert(logic.update(t).toneOn == true);
    }
    logic.setState(SoundState::ArmedIdle);
    assert(logic.update(200).toneOn == false); // flips exactly at 200, unaffected by 200 redundant calls

    // setState(None) silences on the very next tick, without waiting for
    // the current phase to finish.
    logic.setState(SoundState::None);
    assert(logic.update(201).toneOn == false);
    assert(!logic.stateActive());

    cout << "PASS: setState is idempotent; None silences immediately\n";
}

void test_four_queued_events_play_in_order_no_gap() {
    SoundLogic logic;
    logic.setEventPattern(SoundEvent::SystemStart,     {1000, 10, 0, 1});
    logic.setEventPattern(SoundEvent::CalibrationStep, {2000, 20, 0, 1});
    logic.setEventPattern(SoundEvent::ButtonClick,     {3000, 5,  0, 1});
    logic.setEventPattern(SoundEvent::Disarmed,        {4000, 15, 0, 1});

    logic.pushEvent(SoundEvent::SystemStart);
    logic.pushEvent(SoundEvent::CalibrationStep);
    logic.pushEvent(SoundEvent::ButtonClick);
    logic.pushEvent(SoundEvent::Disarmed);

    for (uint32_t t = 0; t <= 49; t++) {
        SoundOutput o = logic.update(t);
        assert(o.toneOn);
        uint16_t expectedFreq =
            (t < 10) ? 1000 :
            (t < 30) ? 2000 :
            (t < 35) ? 3000 : 4000;
        assert(o.freqHz == expectedFreq);
    }
    SoundOutput last = logic.update(50);
    assert(!last.toneOn); // queue drained, no state configured -> silence
    assert(logic.currentEvent() == SoundEvent::kCount);
    assert(logic.queueLength() == 0);
    cout << "PASS: four queued events play back-to-back in order with no gap\n";
}

void test_queue_full_drops_newest_preserves_inflight() {
    SoundLogic logic;
    logic.setEventPattern(SoundEvent::SystemStart,     {1000, 10, 0, 1});
    logic.setEventPattern(SoundEvent::CalibrationStep, {2000, 20, 0, 1});
    logic.setEventPattern(SoundEvent::ButtonClick,     {3000, 5,  0, 1});
    logic.setEventPattern(SoundEvent::Disarmed,        {4000, 15, 0, 1});
    logic.setEventPattern(SoundEvent::LinkLoss,        {9999, 999, 0, 1}); // must never play

    logic.pushEvent(SoundEvent::SystemStart);
    logic.pushEvent(SoundEvent::CalibrationStep);
    logic.pushEvent(SoundEvent::ButtonClick);
    logic.pushEvent(SoundEvent::Disarmed);
    assert(logic.queueLength() == 4);

    logic.pushEvent(SoundEvent::LinkLoss); // queue full -> dropped
    assert(logic.droppedEvents() == 1);
    assert(logic.queueLength() == 4); // unchanged

    bool sawLinkLossFreq = false;
    for (uint32_t t = 0; t <= 60; t++) {
        SoundOutput o = logic.update(t);
        if (o.toneOn && o.freqHz == 9999) sawLinkLossFreq = true;
    }
    assert(!sawLinkLossFreq);
    assert(logic.currentEvent() == SoundEvent::kCount);
    assert(logic.queueLength() == 0);
    cout << "PASS: queue full drops the newest event; in-flight sequence plays intact\n";
}

void test_millis_rollover_state_and_event() {
    {
        SoundLogic logic;
        logic.setStatePattern(SoundState::ArmedIdle, {2000, 200, 200, 0});
        logic.setState(SoundState::ArmedIdle);

        uint32_t nearMax = 0xFFFFFFF0u; // 16ms before wraparound
        assert(logic.update(nearMax).toneOn == true);

        uint32_t justAfterWrap = 20u;   // real elapsed since phase start: 16 + 20 = 36ms
        assert(logic.update(justAfterWrap).toneOn == true); // 36 < 200(onMs)

        uint32_t pastBoundary = 200u;   // real elapsed: 16 + 200 = 216ms >= 200(onMs)
        assert(logic.update(pastBoundary).toneOn == false);
    }
    {
        SoundLogic logic;
        logic.setEventPattern(SoundEvent::ButtonClick, {3000, 50, 0, 1});
        logic.pushEvent(SoundEvent::ButtonClick);

        uint32_t nearMax = 0xFFFFFFE0u; // 32ms before wraparound
        assert(logic.update(nearMax).toneOn == true);

        uint32_t justAfterWrap = 10u;   // real elapsed: 32 + 10 = 42ms
        assert(logic.update(justAfterWrap).toneOn == true); // 42 < 50(onMs)

        uint32_t pastBoundary = 20u;    // real elapsed: 32 + 20 = 52ms >= 50(onMs)
        SoundOutput o = logic.update(pastBoundary);
        assert(o.toneOn == false);
        assert(logic.currentEvent() == SoundEvent::kCount);
    }
    cout << "PASS: millis() rollover handled correctly for both state and event phases\n";
}

void test_tone_transition() {
    assert(computeToneTransition(false, 0, false, 0) == ToneTransition::None);
    assert(computeToneTransition(true, 2000, false, 0) == ToneTransition::TurnOff);
    assert(computeToneTransition(false, 0, true, 2000) == ToneTransition::TurnOn);
    assert(computeToneTransition(true, 2000, true, 2000) == ToneTransition::None);
    assert(computeToneTransition(true, 2000, true, 3000) == ToneTransition::Retune);
    cout << "PASS: computeToneTransition covers all cases; a frequency change while on is Retune\n";
}

int main() {
    test_continuous_state_never_expires();
    test_event_preempts_state_and_resumes_from_start();
    test_setstate_idempotent_and_none_silences_immediately();
    test_four_queued_events_play_in_order_no_gap();
    test_queue_full_drops_newest_preserves_inflight();
    test_millis_rollover_state_and_event();
    test_tone_transition();
    return 0;
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `c++ -std=c++17 -o /tmp/sound_test test/SoundLogicTest.cpp`
Expected: FAIL to compile — `../src/Sound/SoundLogic.h: No such file or directory`

- [ ] **Step 3: Write minimal implementation**

Create `src/Sound/SoundLogic.h`:

```cpp
#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Catalog
// ---------------------------------------------------------------------------

// One named sound's timing/frequency. Registered once via setEventPattern()/
// setStatePattern(); reps == 0 means genuinely continuous (no counter ever
// reaches an end -- this replaces the old "255 = continuous" hack, which was
// actually a literal repeat count and silently stopped after ~102 s).
struct SoundPattern {
    uint16_t freqHz;
    uint16_t onMs;
    uint16_t offMs;
    uint8_t  reps;
};

// Momentary sounds -- always finite, always played from the queue. A
// SoundEvent can never be assigned to the state layer (see SoundState) --
// the two are separate enums so mixing them up is a compile error, not a
// runtime bug.
enum class SoundEvent : uint8_t {
    SystemStart = 0,
    CalibrationStep,
    CalibrationComplete,
    Disarmed,
    ArmingBlocked,
    ButtonClick,
    VolumePreview,
    PowerAlert,
    LinkLoss,
    kCount, // sentinel: "no event" / array size -- never a real event id
};

// Persistent sounds -- exactly one active at a time, recalculated every loop
// tick via setState(). Never queued, never played as a SoundEvent.
enum class SoundState : uint8_t {
    None = 0, // silence
    ArmedIdle,
    kCount,
};

struct SoundOutput {
    bool     toneOn;
    uint16_t freqHz;
};

// ---------------------------------------------------------------------------
// Tone-hardware transition helper
// ---------------------------------------------------------------------------

// What the caller must do to the buzzer hardware to go from the previous
// tick's output to this tick's. Changing frequency while a tone is playing
// produces an audible click on this piezo, so a same-tick frequency change
// (Retune) must be realized as toneOff() followed by toneOn(newFreq), never
// a bare frequency write.
enum class ToneTransition : uint8_t {
    None,
    TurnOff,
    TurnOn,
    Retune,
};

inline ToneTransition computeToneTransition(bool prevOn, uint16_t prevFreq, bool newOn, uint16_t newFreq) {
    if (!prevOn && !newOn) return ToneTransition::None;
    if (prevOn && !newOn)  return ToneTransition::TurnOff;
    if (!prevOn && newOn)  return ToneTransition::TurnOn;
    return (prevFreq == newFreq) ? ToneTransition::None : ToneTransition::Retune;
}

// ---------------------------------------------------------------------------
// Pure layered-audio policy
// ---------------------------------------------------------------------------

// A fixed-size event queue plus one persistent state sound. No Arduino
// deps -- host-testable with `c++ -std=c++17`.
//
// Resolution rules:
//   1. Queue non-empty -> the front event plays; the state sound is
//      preempted immediately (cut mid-tone, no waiting for its phase to
//      finish).
//   2. Queue empty and a state is set -> the state resumes from the start
//      of its cycle (not from wherever it was interrupted).
//   3. pattern.reps == 0 means genuinely continuous -- no counter that can
//      expire mid-flight.
//   4. Queue full -> the newest incoming event is dropped so an in-flight
//      sequence is never truncated; droppedEvents() counts drops.
//   5. No duplicate coalescing -- two real events are two plays.
class SoundLogic {
public:
    static constexpr uint8_t kQueueSize = 4;

    SoundLogic() :
      queueHead_(0), queueCount_(0), currentEvent_(SoundEvent::kCount),
      stateId_(SoundState::None), droppedEvents_(0) {
        for (uint8_t i = 0; i < kQueueSize; i++) queue_[i] = SoundEvent::kCount;
        SoundPattern zero = {0, 0, 0, 0};
        for (uint8_t i = 0; i < static_cast<uint8_t>(SoundEvent::kCount); i++) eventPatterns_[i] = zero;
        for (uint8_t i = 0; i < static_cast<uint8_t>(SoundState::kCount); i++) statePatterns_[i] = zero;
    }

    void setEventPattern(SoundEvent id, SoundPattern pattern) { eventPatterns_[idx(id)] = pattern; }
    void setStatePattern(SoundState id, SoundPattern pattern) { statePatterns_[idx(id)] = pattern; }

    SoundPattern eventPattern(SoundEvent id) const { return eventPatterns_[idx(id)]; }
    SoundPattern statePattern(SoundState id) const { return statePatterns_[idx(id)]; }

    // Enqueues a momentary sound. Drops the newest if the queue is full.
    void pushEvent(SoundEvent id) {
        if (queueCount_ >= kQueueSize) {
            droppedEvents_++;
            return;
        }
        queue_[(queueHead_ + queueCount_) % kQueueSize] = id;
        queueCount_++;
    }

    // Declares the desired persistent state. Idempotent: repeating the same
    // value every tick does not restart the cycle -- only a transition does.
    void setState(SoundState id) {
        if (id == stateId_) return;
        stateId_ = id;
        stateRunner_.stop();
    }

    uint32_t droppedEvents() const { return droppedEvents_; }
    uint8_t  queueLength()   const { return queueCount_; }

    SoundEvent currentEvent()   const { return currentEvent_; }
    SoundState currentStateId() const { return stateId_; }
    bool       stateActive()    const { return stateRunner_.active; }

    // Advances to nowMs (absolute millis()) and returns the tone that
    // should be playing right now.
    SoundOutput update(uint32_t nowMs) {
        for (uint8_t guard = 0; guard <= kQueueSize; guard++) {
            if (!eventRunner_.active && queueCount_ > 0) {
                currentEvent_ = queue_[queueHead_];
                queueHead_ = (queueHead_ + 1) % kQueueSize;
                queueCount_--;
                eventRunner_.start(nowMs);
            }

            if (!eventRunner_.active) break;

            bool toneOn = false;
            bool stillActive = eventRunner_.tick(eventPatterns_[idx(currentEvent_)], nowMs, &toneOn);
            if (stillActive) {
                if (stateRunner_.active) stateRunner_.stop();
                return { toneOn, eventPatterns_[idx(currentEvent_)].freqHz };
            }
            currentEvent_ = SoundEvent::kCount;
            // Event just finished this tick -- loop again to hand off to the
            // next queued event (if any) or the state layer, without waiting
            // for an extra loop() iteration.
        }

        if (stateId_ == SoundState::None) return { false, 0 };

        if (!stateRunner_.active) stateRunner_.start(nowMs);

        bool toneOn = false;
        stateRunner_.tick(statePatterns_[idx(stateId_)], nowMs, &toneOn);
        return { toneOn, statePatterns_[idx(stateId_)].freqHz };
    }

private:
    // Generic on/off/repeat phase timing shared by the event slot and the
    // state slot. Time comparisons use unsigned subtraction, which produces
    // the correct elapsed duration even across a millis() rollover (~49
    // days uptime) as long as the real elapsed time fits in 32 bits.
    struct PhaseRunner {
        bool     active;
        bool     phaseOn;
        uint32_t phaseStartMs;
        uint8_t  repsDone;

        PhaseRunner() : active(false), phaseOn(false), phaseStartMs(0), repsDone(0) {}

        void start(uint32_t nowMs) {
            active = true;
            phaseOn = true;
            phaseStartMs = nowMs;
            repsDone = 0;
        }

        void stop() {
            active = false;
            phaseOn = false;
            repsDone = 0;
        }

        // Returns true while still playing; *toneOnOut reflects this tick.
        bool tick(const SoundPattern &p, uint32_t nowMs, bool *toneOnOut) {
            if (!active) { *toneOnOut = false; return false; }

            uint32_t elapsed = nowMs - phaseStartMs;
            uint32_t phaseDur = phaseOn ? p.onMs : p.offMs;

            if (elapsed >= phaseDur) {
                phaseStartMs = nowMs;
                if (phaseOn) {
                    phaseOn = false;
                    repsDone++;
                    if (p.reps != 0 && repsDone >= p.reps) {
                        stop();
                        *toneOnOut = false;
                        return false;
                    }
                } else {
                    phaseOn = true;
                }
            }

            *toneOnOut = phaseOn;
            return true;
        }
    };

    static uint8_t idx(SoundEvent e) { return static_cast<uint8_t>(e); }
    static uint8_t idx(SoundState s) { return static_cast<uint8_t>(s); }

    SoundEvent  queue_[kQueueSize];
    uint8_t     queueHead_;
    uint8_t     queueCount_;
    SoundEvent  currentEvent_;
    PhaseRunner eventRunner_;

    SoundState  stateId_;
    PhaseRunner stateRunner_;

    SoundPattern eventPatterns_[static_cast<uint8_t>(SoundEvent::kCount)];
    SoundPattern statePatterns_[static_cast<uint8_t>(SoundState::kCount)];

    uint32_t droppedEvents_;
};
```

- [ ] **Step 4: Run test to verify it passes**

Run: `c++ -std=c++17 -o /tmp/sound_test test/SoundLogicTest.cpp && /tmp/sound_test`
Expected:
```
PASS: continuous state alternates for 2000 half-cycles, never expires
PASS: event preempts state immediately and state resumes from cycle start
PASS: setState is idempotent; None silences immediately
PASS: four queued events play back-to-back in order with no gap
PASS: queue full drops the newest event; in-flight sequence plays intact
PASS: millis() rollover handled correctly for both state and event phases
PASS: computeToneTransition covers all cases; a frequency change while on is Retune
```

If any assertion fails, do not adjust the test to match the implementation — the test
encodes a specific bug or design rule from the spec. Re-read the corresponding rule in
`SoundLogic.h`'s class comment and fix the implementation.

- [ ] **Step 5: Commit**

```bash
git add src/Sound/SoundLogic.h test/SoundLogicTest.cpp
git commit -m "feat: add SoundLogic layered event-queue + state policy"
```

---

### Task 4: Shrink `Buzzer` to a pure hardware driver

**Files:**
- Modify: `src/Buzzer/Buzzer.h`
- Modify: `src/Buzzer/Buzzer.cpp`

No test — this class touches `driver/ledc.h` and cannot run on the host. Correctness is
verified by the full-device build in Task 12 and the on-hardware checklist in Task 13.

- [ ] **Step 1: Replace `src/Buzzer/Buzzer.h`**

```cpp
#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include <driver/ledc.h>

// Passive-piezo tone driver via ESP32 LEDC PWM. Pure hardware concern: knows
// how to turn a tone on/off at a given frequency and how loud. All timing,
// pattern, and priority policy lives in Sound/ (see Sound.h / SoundLogic.h).
class Buzzer {
public:
  Buzzer(uint8_t buzzerPin);
  void setup();

  // Re-applies the timer frequency after the global LEDC clock source
  // changes (e.g. when ESP32Servo's esc.attach() forces XTAL on the
  // shared low-speed clock, halving previously-configured frequencies).
  void recalibrate();

  // Sets the output volume as a percentage (0-100), mapped directly to the
  // PWM duty cycle. 0 = silent. Takes effect on the next toneOn().
  void setVolume(uint8_t percent);

  // Starts/retunes the tone at the given frequency. Safe to call while
  // already on -- silently switches frequency without an intermediate
  // toneOff() (callers that need to avoid the audible click of an in-place
  // frequency change should call toneOff() first; see Sound::handle()).
  void toneOn(uint16_t frequencyHz);
  void toneOff();

private:
  uint8_t pin;
  uint8_t pwmChannel;
  uint32_t pwmFrequency;
  uint8_t pwmResolution;
  uint32_t pwmDutyCycle;

  void setPwmOn();
  void setPwmOff();
};

#endif
```

- [ ] **Step 2: Replace `src/Buzzer/Buzzer.cpp`**

```cpp
#include <stdint.h>

#include "Buzzer.h"

namespace {
// Empirical tuning notes for the current hardware:
// - Supply: 3.3 V
// - Transistor stage: BC337
// - Sounder: passive piezo buzzer
// Device testing showed the strongest output near 85% duty cycle. A sweep also
// showed that 2000-2500 Hz stays in the loudest range, while higher
// frequencies lose volume.
constexpr uint16_t kDefaultFrequencyHz = 2300;
constexpr uint8_t kDefaultDutyCycle = 217;  // 85%
}

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),       // Use channel 1 to avoid conflict with ESP32Servo (uses timer 0)
  pwmFrequency(kDefaultFrequencyHz),
  pwmResolution(8),    // 8-bit resolution (0-255)
  pwmDutyCycle(kDefaultDutyCycle) {
}

void Buzzer::setup() {
  ledc_timer_config_t ledc_timer = {};
  ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
  ledc_timer.timer_num = LEDC_TIMER_1;  // Changed from LEDC_TIMER_0 to avoid ESP32Servo conflict
  ledc_timer.freq_hz = pwmFrequency;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel = {};
  ledc_channel.gpio_num = (gpio_num_t)pin;
  ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel = (ledc_channel_t)pwmChannel;
  ledc_channel.timer_sel = LEDC_TIMER_1;  // Changed from LEDC_TIMER_0 to avoid ESP32Servo conflict
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.duty = 0;
  ledc_channel.hpoint = 0;
  ledc_channel.flags.output_invert = 0;
  ledc_channel_config(&ledc_channel);

  setPwmOff();
}

void Buzzer::recalibrate() {
  // ESP32Servo's esc.attach() can force the LEDC low-speed clock to XTAL
  // (40 MHz) so it can hit 50 Hz at 16-bit. Our timer was configured under
  // the previous clock source, so its divider now produces a different
  // frequency. Re-apply pwmFrequency so the divider is recomputed against
  // the current clock.
  ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, pwmFrequency);
}

void Buzzer::setVolume(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  // Map 0-100% directly to the 8-bit duty cycle (0-255). 0% = silent.
  pwmDutyCycle = (uint32_t)percent * 255 / 100;
}

void Buzzer::toneOn(uint16_t frequencyHz) {
  if (frequencyHz == 0) {
    frequencyHz = pwmFrequency;
  }
  if (frequencyHz != pwmFrequency) {
    pwmFrequency = frequencyHz;
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, pwmFrequency);
  }
  setPwmOn();
}

void Buzzer::toneOff() {
  setPwmOff();
}

void Buzzer::setPwmOn() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel, pwmDutyCycle);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel);
}

void Buzzer::setPwmOff() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel);
}
```

- [ ] **Step 3: Commit**

The build will not succeed yet (nothing calls the new API; `config.h` still only knows
about the old `Buzzer` methods indirectly through files not yet updated) — that's
expected until Task 6 wires `Sound` in. Commit anyway so each task stays isolated and
`git bisect`-able; Task 6's commit is where the build becomes green again.

```bash
git add src/Buzzer/Buzzer.h src/Buzzer/Buzzer.cpp
git commit -m "refactor: shrink Buzzer to a pure LEDC tone driver"
```

---

### Task 5: `Sound` component

**Files:**
- Create: `src/Sound/Sound.h`
- Create: `src/Sound/Sound.cpp`

- [ ] **Step 1: Create `src/Sound/Sound.h`**

```cpp
#ifndef SOUND_H
#define SOUND_H

#include <Arduino.h>
#include "../Buzzer/Buzzer.h"
#include "SoundLogic.h"

// One entry in the beep event ring -- for web telemetry transport.
struct BeepEvent {
  uint32_t seq;       // monotonic counter; 0 = empty slot
  uint16_t frequency; // Hz
  uint16_t onMs;      // on duration
  uint16_t offMs;     // off/pause duration between reps
  uint8_t  reps;      // 0 = continuous
  uint8_t  layer;     // 0 = event (queue), 1 = state (persistent)
  bool     active;    // true = started, false = stopped
};

// Sound policy component: owns the named-pattern catalog, drives SoundLogic
// each tick, and translates its output into Buzzer hardware calls. See
// SoundLogic.h for the queue/state resolution rules.
class Sound {
public:
  static constexpr uint8_t kRingSize = 8;

  Sound();
  void handle();

  // Enqueues a momentary sound (see SoundEvent for the catalog).
  void play(SoundEvent id);

  // Declares the desired persistent state sound. Idempotent.
  void setState(SoundState id);

  // Returns events in ascending seq order (oldest first), up to maxCount.
  // Returns the number written into buf.
  uint8_t getBeepEvents(BeepEvent* buf, uint8_t maxCount) const;

private:
  SoundLogic logic_;

  SoundEvent prevEvent_;
  bool       prevStateActive_;
  bool       lastToneOn_;
  uint16_t   lastFreqHz_;

  BeepEvent beepRing_[kRingSize];
  uint8_t   beepWriteIdx_;
  uint8_t   beepCount_;
  uint32_t  beepSeq_;

  void pushBeepEvent(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t reps, uint8_t layer, bool active);
};

#endif
```

- [ ] **Step 2: Create `src/Sound/Sound.cpp`**

```cpp
#include "Sound.h"

extern Buzzer buzzer;

namespace {
// Empirical tuning: 2300 Hz for general UX beeps, 2000 Hz for the armed
// alert (stands out immediately), 2500 Hz for the power alert and the new
// link-loss warning. See Buzzer.cpp for the duty-cycle/frequency sweep notes.
constexpr uint16_t kDefaultFreqHz = 2300;
constexpr uint16_t kArmedFreqHz   = 2000;
constexpr uint16_t kPowerFreqHz   = 2500;
}

Sound::Sound() :
  prevEvent_(SoundEvent::kCount),
  prevStateActive_(false),
  lastToneOn_(false),
  lastFreqHz_(0),
  beepRing_{},
  beepWriteIdx_(0),
  beepCount_(0),
  beepSeq_(0) {

  // Same envelopes as the original beepXxx() methods (see git history of
  // Buzzer.cpp for the empirical rationale behind each).
  logic_.setEventPattern(SoundEvent::SystemStart,         {kDefaultFreqHz, 150, 80,  3});
  logic_.setEventPattern(SoundEvent::CalibrationStep,     {kDefaultFreqHz, 80,  60,  2});
  logic_.setEventPattern(SoundEvent::CalibrationComplete, {kDefaultFreqHz, 200, 80,  3});
  logic_.setEventPattern(SoundEvent::Disarmed,            {kDefaultFreqHz, 250, 150, 2});
  logic_.setEventPattern(SoundEvent::ArmingBlocked,       {kDefaultFreqHz, 60,  40,  5});
  logic_.setEventPattern(SoundEvent::ButtonClick,         {kDefaultFreqHz, 50,  0,   1});
  logic_.setEventPattern(SoundEvent::VolumePreview,       {kDefaultFreqHz, 120, 0,   1});
  logic_.setEventPattern(SoundEvent::PowerAlert,          {kPowerFreqHz,   100, 60,  3});
  // New: audible warning during the wireless failsafe ramp window (500ms-3s
  // of link loss), where today there is silence. Short and sharp so it's
  // unmistakably different from every other pattern.
  logic_.setEventPattern(SoundEvent::LinkLoss,            {kPowerFreqHz,   80,  60,  2});

  // reps=0: genuinely continuous. This is the fix for the bug where
  // reps=255 was a literal repeat count and silenced itself after ~102s.
  logic_.setStatePattern(SoundState::ArmedIdle, {kArmedFreqHz, 200, 200, 0});
}

void Sound::play(SoundEvent id) {
  logic_.pushEvent(id);
}

void Sound::setState(SoundState id) {
  logic_.setState(id);
}

void Sound::handle() {
  uint32_t now = millis();
  SoundOutput out = logic_.update(now);

  SoundEvent ev = logic_.currentEvent();
  if (ev != prevEvent_ && ev != SoundEvent::kCount) {
    SoundPattern p = logic_.eventPattern(ev);
    pushBeepEvent(p.freqHz, p.onMs, p.offMs, p.reps, /*layer=*/0, /*active=*/true);
  }
  prevEvent_ = ev;

  bool stActive = logic_.stateActive();
  if (stActive && !prevStateActive_) {
    SoundPattern p = logic_.statePattern(logic_.currentStateId());
    pushBeepEvent(p.freqHz, p.onMs, p.offMs, p.reps, /*layer=*/1, /*active=*/true);
  } else if (!stActive && prevStateActive_) {
    pushBeepEvent(0, 0, 0, 0, /*layer=*/1, /*active=*/false);
  }
  prevStateActive_ = stActive;

  switch (computeToneTransition(lastToneOn_, lastFreqHz_, out.toneOn, out.freqHz)) {
    case ToneTransition::TurnOff:
      buzzer.toneOff();
      break;
    case ToneTransition::TurnOn:
      buzzer.toneOn(out.freqHz);
      break;
    case ToneTransition::Retune:
      buzzer.toneOff();
      buzzer.toneOn(out.freqHz);
      break;
    case ToneTransition::None:
      break;
  }
  lastToneOn_ = out.toneOn;
  lastFreqHz_ = out.freqHz;
}

void Sound::pushBeepEvent(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t reps, uint8_t layer, bool active) {
  beepRing_[beepWriteIdx_] = { ++beepSeq_, freq, onMs, offMs, reps, layer, active };
  beepWriteIdx_ = (beepWriteIdx_ + 1) % kRingSize;
  if (beepCount_ < kRingSize) beepCount_++;
}

uint8_t Sound::getBeepEvents(BeepEvent* buf, uint8_t maxCount) const {
  uint8_t count = (beepCount_ < maxCount) ? beepCount_ : maxCount;
  uint8_t start = (beepWriteIdx_ + kRingSize - beepCount_) % kRingSize;
  for (uint8_t i = 0; i < count; i++) {
    buf[i] = beepRing_[(start + i) % kRingSize];
  }
  return count;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/Sound/Sound.h src/Sound/Sound.cpp
git commit -m "feat: add Sound component gluing SoundLogic to Buzzer hardware"
```

---

### Task 6: Wire `Sound` into `config.h` / `config.cpp`

**Files:**
- Modify: `src/config.h:6` (add include), `:49` (add extern), `:144` (add constant)
- Modify: `src/config.cpp:13` (instantiate)

- [ ] **Step 1: Add the include and extern declaration to `src/config.h`**

At line 6, add the new include right after the existing `Buzzer/Buzzer.h` one:

```cpp
#include "Buzzer/Buzzer.h"
#include "Sound/Sound.h"
```

At line 49, add the new global right after `buzzer`:

```cpp
extern Buzzer buzzer;
extern Sound sound;
```

- [ ] **Step 2: Add the link-loss interval constant**

Find the `POWER ALERT` section (around line 143-144) and add the new constant right
after it:

```cpp
// ========== POWER ALERT ==========
#define POWER_ALERT_BEEP_INTERVAL_MS 10000

// ========== SOUND ==========
// Repeat interval for the audible warning during the wireless failsafe ramp
// window (500ms-3s of link loss). Short enough to give several warnings
// before the 3s disarm.
#define SOUND_LINK_LOSS_INTERVAL_MS 500
```

- [ ] **Step 3: Instantiate `sound` in `src/config.cpp`**

At line 13, add right after `Buzzer buzzer(BUZZER_PIN);`:

```cpp
Buzzer buzzer(BUZZER_PIN);
Sound sound;
```

- [ ] **Step 4: Commit**

```bash
git add src/config.h src/config.cpp
git commit -m "feat: wire the Sound component into config globals"
```

Building will still fail until the remaining `beepXxx()` call sites are updated (Tasks
7-11) — the old `Buzzer` methods no longer exist. That's expected; the build goes green
again at the end of Task 11.

---

### Task 7: `main.h` / `main.cpp` — loop order, declarative sound state, link-loss warning

**Files:**
- Modify: `src/main.h:15`
- Modify: `src/main.cpp:17` (include), `:129-130` (setup), `:143` (loop order), `:159-165`
  (failsafe block), `:185` (call site), `:238-271` (function bodies)

- [ ] **Step 1: Update `src/main.h`**

Change:

```cpp
bool isMotorRunning();
void handleArmedBeep();
```

to:

```cpp
bool isMotorRunning();
void updateSoundState();
```

- [ ] **Step 2: Add the `Sound` include in `src/main.cpp`**

At line 17, add right after the `Buzzer/Buzzer.h` include:

```cpp
#include "Buzzer/Buzzer.h"
#include "Sound/Sound.h"
```

- [ ] **Step 3: Update `setup()` — line 129**

Change:

```cpp
  buzzer.beepSystemStart();
  remoteLink.requestBeep(RemoteBeep::SystemStart);
```

to:

```cpp
  sound.play(SoundEvent::SystemStart);
  remoteLink.requestBeep(RemoteBeep::SystemStart);
```

- [ ] **Step 4: Update `loop()`'s first line — line 143**

Change:

```cpp
  // buzzer.handle() runs first so a beep that finished during the previous
  // iteration is silenced before any potentially slow component runs.
  buzzer.handle();
```

to:

```cpp
  // sound.handle() runs first so a tone that finished during the previous
  // iteration is silenced before any potentially slow component runs.
  sound.handle();
```

- [ ] **Step 5: Update the wireless failsafe block — lines 159-165**

Change:

```cpp
  // Wireless failsafe: prolonged link loss disarms (the ramp-to-zero case is
  // handled in the throttle ReadFn feeding 0). See RemoteLinkLogic.
  if (settings.getThrottleSource() == ThrottleSourceWireless &&
      throttle.isArmed() &&
      remoteLink.failsafe(true, millis()) == FailsafeAction::Disarm) {
    throttle.setDisarmed();
  }
```

to:

```cpp
  // Wireless failsafe: prolonged link loss disarms (the ramp-to-zero case is
  // handled in the throttle ReadFn feeding 0). See RemoteLinkLogic. The ramp
  // window (500ms-3s) also gets an audible warning -- previously silent --
  // so a pilot not looking at the remote still knows something is wrong.
  static PeriodicTrigger linkLossTrigger;
  if (settings.getThrottleSource() == ThrottleSourceWireless && throttle.isArmed()) {
    FailsafeAction action = remoteLink.failsafe(true, millis());
    if (linkLossTrigger.update(action == FailsafeAction::RampToZero, millis(), SOUND_LINK_LOSS_INTERVAL_MS)) {
      sound.play(SoundEvent::LinkLoss);
    }
    if (action == FailsafeAction::Disarm) {
      throttle.setDisarmed();
    }
  }
```

This needs the `PeriodicTrigger` include, which is already available transitively
through `Sound/Sound.h` -> `Sound/SoundLogic.h` -- no, `SoundLogic.h` does not include
`PeriodicTrigger.h` (they're independent headers). Add it explicitly next to the other
includes added in Step 2:

```cpp
#include "Buzzer/Buzzer.h"
#include "Sound/Sound.h"
#include "Sound/PeriodicTrigger.h"
```

- [ ] **Step 6: Update the call site — line 185**

Change:

```cpp
  handleEsc();
  handleArmedBeep();
  powerAlert.handle();
```

to:

```cpp
  handleEsc();
  updateSoundState();
  powerAlert.handle();
```

- [ ] **Step 7: Rewrite `isMotorRunning()` and replace `handleArmedBeep()`**

Change:

```cpp
bool isMotorRunning()
{
    return throttle.getThrottlePercentage() > 1 && throttle.isArmed();
}

void handleArmedBeep()
{
    static bool wasArmed = false;
    static bool wasMotorRunning = false;

    bool isArmed = throttle.isArmed();
    bool motorRunning = isMotorRunning();

    // Controller buzzer: beep when armed + motor stopped (local safety alert).
    if (isArmed && !motorRunning && (!wasArmed || wasMotorRunning)) {
        buzzer.beepArmedAlert();
    }
    if ((!isArmed || motorRunning) && wasArmed && !wasMotorRunning) {
        buzzer.stop();
    }

    // Remote buzzer: beep continuously while armed, stop on disarm.
    // Decoupled from motor state to avoid Stop/Armed oscillation from
    // throttle noise crossing the isMotorRunning threshold.
    if (isArmed && !wasArmed) {
        remoteLink.requestBeep(RemoteBeep::Armed);
    }
    if (!isArmed && wasArmed) {
        remoteLink.requestBeep(RemoteBeep::Disarmed);
    }

    wasArmed = isArmed;
    wasMotorRunning = motorRunning;
}
```

to:

```cpp
bool isMotorRunning()
{
    // Uses the same 2%/1% engage hysteresis Power already gates on
    // (ThrottleEngagementLogic), instead of a raw 1% threshold. The raw
    // threshold let Hall-sensor noise at idle flip this repeatedly, which
    // re-triggered the armed alert from scratch on every flip.
    return throttle.isArmed() && throttle.isEngaged();
}

void updateSoundState()
{
    static bool wasArmed = false;
    static bool wasArmedIdle = false;

    bool isArmed = throttle.isArmed();
    bool armedIdle = isArmed && !isMotorRunning();

    // Controller buzzer: declarative -- recalculated every tick from the
    // current state, so a preempting event (button click, power alert,
    // ...) can never permanently silence it. See Sound/SoundLogic.h.
    sound.setState(armedIdle ? SoundState::ArmedIdle : SoundState::None);

    // Remote buzzer: requestBeep() is a one-shot command, so it still needs
    // edge detection. Now mirrors the same armed+stopped rule as the
    // controller (isMotorRunning() uses the same engage hysteresis), so the
    // two buzzers agree on when to sound -- previously the remote beeped on
    // armed alone, ignoring motor state.
    if (armedIdle && !wasArmedIdle) {
        remoteLink.requestBeep(RemoteBeep::Armed);
    }
    if (!armedIdle && wasArmedIdle && isArmed) {
        remoteLink.requestBeep(RemoteBeep::Stop);
    }
    if (!isArmed && wasArmed) {
        remoteLink.requestBeep(RemoteBeep::Disarmed);
    }

    wasArmed = isArmed;
    wasArmedIdle = armedIdle;
}
```

- [ ] **Step 8: Grep for any other callers before moving on**

Run: `grep -rn "isMotorRunning\|handleArmedBeep\|beepArmedAlert\|buzzer\.stop\(\)" src/`
Expected: only the definitions/call sites just edited, plus the `hourMeter.handle()` call
at (now-shifted) line ~167 that already calls `isMotorRunning()` -- no leftover
references to `handleArmedBeep` or the removed `Buzzer::stop()`/`beepArmedAlert()`
methods.

- [ ] **Step 9: Commit**

```bash
git add src/main.h src/main.cpp
git commit -m "refactor: make armed-alert sound declarative, add link-loss warning"
```

---

### Task 8: `Button.cpp`

**Files:**
- Modify: `src/Button/Button.cpp`

- [ ] **Step 1: Swap the include/extern and the call site**

Change:

```cpp
#include "../config.h"
#include "Button.h"
#include "../Throttle/Throttle.h"
#include "../Buzzer/Buzzer.h"
#include "../RemoteLink/RemoteLink.h"
#include "../Settings/Settings.h"
#include "../main.h"

extern Throttle throttle;
extern Buzzer buzzer;
extern Settings settings;
```

to:

```cpp
#include "../config.h"
#include "Button.h"
#include "../Throttle/Throttle.h"
#include "../Sound/Sound.h"
#include "../RemoteLink/RemoteLink.h"
#include "../Settings/Settings.h"
#include "../main.h"

extern Throttle throttle;
extern Sound sound;
extern Settings settings;
```

Change:

```cpp
      if (buttonWasClicked) {
        buzzer.beepButtonClick();
```

to:

```cpp
      if (buttonWasClicked) {
        sound.play(SoundEvent::ButtonClick);
```

- [ ] **Step 2: Commit**

```bash
git add src/Button/Button.cpp
git commit -m "refactor: Button plays SoundEvent::ButtonClick instead of Buzzer"
```

---

### Task 9: `Throttle.h` / `Throttle.cpp`

**Files:**
- Modify: `src/Throttle/Throttle.h:4`
- Modify: `src/Throttle/Throttle.cpp:84,127,198,203,220`

- [ ] **Step 1: Drop the unused `Buzzer.h` include from `src/Throttle/Throttle.h`**

`Throttle` never references the `Buzzer` type directly -- `Throttle.cpp` reaches the
global `buzzer` (soon `sound`) purely through `../config.h`, which it already includes.
Change:

```cpp
#include "../Buzzer/Buzzer.h"
#include "../config.h"
#include "ThrottleEngagementLogic.h"
```

to:

```cpp
#include "../config.h"
#include "ThrottleEngagementLogic.h"
```

- [ ] **Step 2: Update the five call sites in `src/Throttle/Throttle.cpp`**

Line 84, change:
```cpp
        buzzer.beepCalibrationStep();
```
to:
```cpp
        sound.play(SoundEvent::CalibrationStep);
```

Line 127, change:
```cpp
        buzzer.beepCalibrationComplete();
```
to:
```cpp
        sound.play(SoundEvent::CalibrationComplete);
```

Line 198, change:
```cpp
    buzzer.beepArmingBlocked();  // Warning: must calibrate throttle first
```
to:
```cpp
    sound.play(SoundEvent::ArmingBlocked);  // Warning: must calibrate throttle first
```

Line 203, change:
```cpp
    buzzer.beepArmingBlocked();
```
to:
```cpp
    sound.play(SoundEvent::ArmingBlocked);
```

Line 220, change:
```cpp
  buzzer.beepDisarmed();
```
to:
```cpp
  sound.play(SoundEvent::Disarmed);
```

- [ ] **Step 3: Commit**

```bash
git add src/Throttle/Throttle.h src/Throttle/Throttle.cpp
git commit -m "refactor: Throttle plays SoundEvent patterns instead of Buzzer"
```

---

### Task 10: `PowerAlert.cpp`

**Files:**
- Modify: `src/PowerAlert/PowerAlert.cpp`

- [ ] **Step 1: Swap the include/extern and the call site**

Change:

```cpp
#include "PowerAlert.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Buzzer/Buzzer.h"
#include "../RemoteLink/RemoteLink.h"
#include "../RemoteLink/RemoteLinkProtocol.h"

extern Throttle throttle;
extern Power power;
extern Buzzer buzzer;
extern RemoteLink remoteLink;

PowerAlert::PowerAlert() : seq_(0), activeCauses_(0) {}

void PowerAlert::handle() {
    activeCauses_ = power.getActiveLimitCauses();

    if (logic_.update(throttle.isArmed(), activeCauses_, millis(), POWER_ALERT_BEEP_INTERVAL_MS)) {
        seq_++;
        buzzer.beepPowerAlert();
        remoteLink.requestBeep(RemoteBeep::PowerAlert);
    }
}
```

to:

```cpp
#include "PowerAlert.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Power/Power.h"
#include "../Sound/Sound.h"
#include "../RemoteLink/RemoteLink.h"
#include "../RemoteLink/RemoteLinkProtocol.h"

extern Throttle throttle;
extern Power power;
extern Sound sound;
extern RemoteLink remoteLink;

PowerAlert::PowerAlert() : seq_(0), activeCauses_(0) {}

void PowerAlert::handle() {
    activeCauses_ = power.getActiveLimitCauses();

    if (logic_.update(throttle.isArmed(), activeCauses_, millis(), POWER_ALERT_BEEP_INTERVAL_MS)) {
        seq_++;
        sound.play(SoundEvent::PowerAlert);
        remoteLink.requestBeep(RemoteBeep::PowerAlert);
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add src/PowerAlert/PowerAlert.cpp
git commit -m "refactor: PowerAlert plays SoundEvent::PowerAlert instead of Buzzer"
```

---

### Task 11: `ControllerWebServer.cpp` — ring buffer rename + `layer` field

**Files:**
- Modify: `src/WebServer/ControllerWebServer.cpp:296,786-793`

- [ ] **Step 1: Volume-preview beep — around line 296**

Change:

```cpp
        buzzer.setVolume((uint8_t)volume);
        buzzer.beepVolumePreview();
        request->send(200, "application/json", "{\"ok\":true}");
```

to:

```cpp
        buzzer.setVolume((uint8_t)volume);
        sound.play(SoundEvent::VolumePreview);
        request->send(200, "application/json", "{\"ok\":true}");
```

(`buzzer.setVolume()` is unchanged -- volume is duty cycle, a hardware concern that stays
on `Buzzer`.)

- [ ] **Step 2: Telemetry ring buffer read — around lines 785-793**

Change:

```cpp
        {
            BeepEvent evBuf[Buzzer::kRingSize];
            uint8_t evCount = buzzer.getBeepEvents(evBuf, Buzzer::kRingSize);
            JsonArray buzzerArr = doc.createNestedArray("buzzer");
            for (uint8_t i = 0; i < evCount; i++) {
                JsonObject buzzerObj = buzzerArr.createNestedObject();
                buzzerObj["seq"]    = evBuf[i].seq;
                buzzerObj["freq"]   = evBuf[i].frequency;
                buzzerObj["onMs"]   = evBuf[i].onMs;
                buzzerObj["offMs"]  = evBuf[i].offMs;
                buzzerObj["reps"]   = evBuf[i].reps;
                buzzerObj["active"] = evBuf[i].active;
            }
        }
```

to:

```cpp
        {
            BeepEvent evBuf[Sound::kRingSize];
            uint8_t evCount = sound.getBeepEvents(evBuf, Sound::kRingSize);
            JsonArray buzzerArr = doc.createNestedArray("buzzer");
            for (uint8_t i = 0; i < evCount; i++) {
                JsonObject buzzerObj = buzzerArr.createNestedObject();
                buzzerObj["seq"]    = evBuf[i].seq;
                buzzerObj["freq"]   = evBuf[i].frequency;
                buzzerObj["onMs"]   = evBuf[i].onMs;
                buzzerObj["offMs"]  = evBuf[i].offMs;
                buzzerObj["reps"]   = evBuf[i].reps;
                buzzerObj["layer"]  = evBuf[i].layer;
                buzzerObj["active"] = evBuf[i].active;
            }
        }
```

- [ ] **Step 3: Grep for any remaining `Buzzer::` or `buzzer.beep`/`buzzer.getBeepEvents`/`buzzer.stop` references**

Run: `grep -rn "Buzzer::kRingSize\|buzzer\.beep\|buzzer\.getBeepEvents\|buzzer\.stop(" src/`
Expected: no matches. If any remain, apply the same rename pattern used above.

- [ ] **Step 4: Commit**

```bash
git add src/WebServer/ControllerWebServer.cpp
git commit -m "refactor: web server reads the Sound ring buffer, adds layer field"
```

---

### Task 12: Build both targets and run every host test

**Files:** none (verification only)

- [ ] **Step 1: Run every host-tested logic file**

```bash
c++ -std=c++17 -o /tmp/periodic_test test/PeriodicTriggerTest.cpp && /tmp/periodic_test
c++ -std=c++17 -o /tmp/sound_test test/SoundLogicTest.cpp && /tmp/sound_test
c++ -std=c++17 -o /tmp/power_alert_test test/PowerAlertLogicTest.cpp && /tmp/power_alert_test
c++ -std=c++17 -o /tmp/remote_link_test test/RemoteLinkLogicTest.cpp && /tmp/remote_link_test
c++ -std=c++17 -o /tmp/throttle_engage_test test/ThrottleEngagementLogicTest.cpp && /tmp/throttle_engage_test
c++ -std=c++17 -o /tmp/jk_bms_test test/JkBmsParserTest.cpp && /tmp/jk_bms_test
```

Expected: every one exits 0 with `PASS:` lines and no assertion failures. Running the
pre-existing suites (not just the new ones) confirms the `PowerAlertLogic` rewrite in
Task 2 and the `isMotorRunning()`/hour-meter change in Task 7 didn't silently break
something these tests already cover.

- [ ] **Step 2: Build both firmware targets**

```bash
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
```

Expected: `SUCCESS` for both. If either fails, the error will point at a leftover
`buzzer.beepXxx()`/`Buzzer::kRingSize` call or a missing include -- re-run the grep from
Task 11 Step 3 with a broader pattern (`buzzer\.beep|Buzzer::kRingSize`) across the whole
`src/` tree to find it.

- [ ] **Step 3: Check flash/RAM delta against `main`**

The `pio run` output prints RAM/flash usage. Compare against a build of `main` if in
doubt; the spec's expectation is a small net *decrease* (Buzzer shrank by more than Sound
grew), not an increase. This target has been heap-constrained before (commit 61fa202) --
if usage grew noticeably, stop and investigate before continuing (likely cause: an
unintended duplicate of the pattern catalog, or a container that isn't a fixed-size
array).

- [ ] **Step 4: No commit for this task** -- it's verification only. If Steps 1-3 all
  passed, proceed to Task 13. If something needed a fix, commit that fix with a message
  describing what was actually wrong (e.g. `fix: missing Sound.h include in X`), then
  re-run Steps 1-2 before moving on.

---

### Task 13: Browser mirror (`TelemetryPage.h`)

**Files:**
- Modify: `src/WebServer/Pages/TelemetryPage.h`

- [ ] **Step 1: Add state-tracking variables**

Find the block of `let` declarations around line 150-158:

```js
let audioCtx = null;
let gainMaster = null;
let soundMuted = localStorage.getItem('bzMuted') !== '0';  // default: muted
let bzStopLoopFlag = false;
let bzActiveOsc = null;
let bzActiveGain = null;
let bzLastSeq = -1;
let bzPrimed = false;
let bzLoopIsRunning = false;
```

Add two more variables after `bzLoopIsRunning`:

```js
let bzStateOn = false;       // is the state-layer (continuous) loop currently playing
let bzStatePattern = null;   // {freq, onMs, offMs} of the active state, for resuming after an event
```

- [ ] **Step 2: Replace `bzPlayQueue`**

Find (around line 241-259):

```js
const bzPlayQueue = (events) => {
    if (!audioCtx) return;
    let t = audioCtx.currentTime;
    for (const ev of events) {
        if (!ev.active) {
            bzStopLoop();
            t = audioCtx.currentTime;
        } else if (ev.reps === 255) {
            bzStartLoop(ev.freq, ev.onMs, ev.offMs);
            t = audioCtx.currentTime;
        } else {
            if (bzLoopIsRunning) {
                bzStopLoop();
                t = audioCtx.currentTime;
            }
            t = bzScheduleOnce(ev.freq, ev.onMs, ev.offMs, ev.reps, t);
        }
    }
};
```

Replace with:

```js
const bzPlayQueue = (events) => {
    if (!audioCtx) return;
    let t = audioCtx.currentTime;
    let interruptedState = false;

    for (const ev of events) {
        if (ev.layer === 1) {
            // State layer: only act on a real transition. A repeated
            // "active:true" for an already-running state (or a repeated
            // "active:false" for an already-stopped one) is ignored so the
            // loop is never restarted mid-cycle.
            if (ev.active && !bzStateOn) {
                bzStartLoop(ev.freq, ev.onMs, ev.offMs);
                bzStateOn = true;
                bzStatePattern = { freq: ev.freq, onMs: ev.onMs, offMs: ev.offMs };
                t = audioCtx.currentTime;
            } else if (!ev.active && bzStateOn) {
                bzStopLoop();
                bzStateOn = false;
                bzStatePattern = null;
                t = audioCtx.currentTime;
            }
            continue;
        }

        // Event layer: a queued, finite pattern. If the state loop is
        // running, pause it so the event can be heard, then resume it
        // after (see below) -- accepting a small timing imprecision on the
        // resume rather than trying to reproduce the firmware's exact
        // sample-accurate preemption in the browser.
        if (bzLoopIsRunning) {
            bzStopLoop();
            interruptedState = true;
            t = audioCtx.currentTime;
        }
        t = bzScheduleOnce(ev.freq, ev.onMs, ev.offMs, ev.reps, t);
    }

    if (bzStateOn && (interruptedState || !bzLoopIsRunning)) {
        const pattern = bzStatePattern;
        const delayMs = Math.max(0, (t - audioCtx.currentTime) * 1000);
        setTimeout(() => {
            if (bzStateOn && !bzLoopIsRunning) bzStartLoop(pattern.freq, pattern.onMs, pattern.offMs);
        }, delayMs);
    }
};
```

- [ ] **Step 3: Replace `bzProcessEvents`**

Find (around line 261-272):

```js
const bzProcessEvents = (events) => {
    if (!bzPrimed) {
        bzLastSeq = (events && events.length > 0) ? events[events.length - 1].seq : -1;
        bzPrimed = true;
        return;
    }
    if (!events || !events.length) return;
    const fresh = events.filter(ev => ev.seq > bzLastSeq);
    if (!fresh.length) return;
    bzLastSeq = fresh[fresh.length - 1].seq;
    bzPlayQueue(fresh);
};
```

Replace with:

```js
const bzProcessEvents = (events) => {
    if (!bzPrimed) {
        bzPrimed = true;
        if (events && events.length) {
            bzLastSeq = events[events.length - 1].seq;
            // Skip replaying queued (layer 0) history, but apply the last
            // known state (layer 1) so the page starts in sync with
            // whatever the device is already doing -- e.g. opening the
            // page while armed and stopped should start the continuous
            // alert immediately instead of staying silent.
            for (let i = events.length - 1; i >= 0; i--) {
                if (events[i].layer === 1) {
                    bzPlayQueue([events[i]]);
                    break;
                }
            }
        } else {
            bzLastSeq = -1;
        }
        return;
    }
    if (!events || !events.length) return;
    const fresh = events.filter(ev => ev.seq > bzLastSeq);
    if (!fresh.length) return;
    bzLastSeq = fresh[fresh.length - 1].seq;
    bzPlayQueue(fresh);
};
```

- [ ] **Step 4: Grep for any other `reps === 255` or `reps == 255` references**

Run: `grep -n "255" src/WebServer/Pages/TelemetryPage.h`
Expected: no matches related to buzzer reps (the firmware no longer ever sends 255; any
other unrelated 255 in the file, if present, is not a buzzer concern).

- [ ] **Step 5: Manual browser check (defer full verification to Task 15)**

This step cannot be verified from the command line -- host tests don't cover browser JS
in this repo, and there's no headless test harness here (`test/CLAUDE.md` only covers
host-native C++ logic). Confirm only that the file still parses as valid JS by starting
the device and loading `/telemetry` with the browser console open, watching for syntax
errors. Full audio-correctness verification happens on real hardware in Task 15.

- [ ] **Step 6: Commit**

```bash
git add src/WebServer/Pages/TelemetryPage.h
git commit -m "feat: layered browser mirror for the buzzer event stream"
```

---

### Task 14: Update documentation

**Files:**
- Modify: `src/CLAUDE.md` (Buzzer/PowerAlert sections)
- Modify: `CLAUDE.md` (structure tree, telemetry page description, remote-link description)
- Modify: `test/CLAUDE.md` (new test files)

- [ ] **Step 1: Replace the `Buzzer` section of `src/CLAUDE.md`**

Find the `### Buzzer — \`Buzzer/\`` section and replace its entire contents (including
the paragraph about `getBeepEvents()`) with:

```markdown
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
(`SoundEvent::LinkLoss`, fired via `PeriodicTrigger` in `main.cpp`'s failsafe block).

`sound.getBeepEvents()` returns a ring buffer of up to 8 `BeepEvent` snapshots (oldest
first), each `{seq, frequency, onMs, offMs, reps, layer, active}` -- `layer` is 0 for a
queued event, 1 for the state layer. The web server reads this to include a `buzzer`
array in `/api/telemetry`. The state layer only publishes on a real transition (one entry
starting it, one stopping it), never repeatedly. On the first successful poll the
telemetry page primes `bzLastSeq` to the highest seq (skipping queued-event replay) but
still applies the most recent state event, so the page starts in sync with whatever the
device is already doing. Subsequent polls play fresh queued events immediately and toggle
the state loop on transition, pausing/resuming it around queued events.
```

- [ ] **Step 2: Update the `PowerAlert` section of `src/CLAUDE.md`**

Find:

```markdown
The component (`PowerAlert.cpp`) reads `power.getActiveLimitCauses()` + `throttle.isArmed()`, calls `buzzer.beepPowerAlert()` + `remoteLink.requestBeep(RemoteBeep::PowerAlert)` on entry and every `POWER_ALERT_BEEP_INTERVAL_MS` (10 s) while limited.
```

Replace with:

```markdown
The component (`PowerAlert.cpp`) reads `power.getActiveLimitCauses()` + `throttle.isArmed()`, calls `sound.play(SoundEvent::PowerAlert)` + `remoteLink.requestBeep(RemoteBeep::PowerAlert)` on entry and every `POWER_ALERT_BEEP_INTERVAL_MS` (10 s) while limited. `PowerAlertLogic` is a thin wrapper over the shared `PeriodicTrigger` (see `Sound/`).
```

- [ ] **Step 3: Update `CLAUDE.md`'s directory tree**

Find:

```
├── Buzzer/                   # Non-blocking PWM beep patterns
```

Replace with:

```
├── Buzzer/                   # LEDC PWM tone driver (hardware only)
├── Sound/                    # Layered audio policy: event queue + persistent state
```

(Keep alphabetical-ish ordering consistent with the surrounding tree -- insert `Sound/`
where it naturally falls near `Settings/`/`Telemetry/` in that listing.)

- [ ] **Step 4: Update `CLAUDE.md`'s telemetry-page paragraph**

Find:

```markdown
The **telemetry page** (`/telemetry`) polls `/api/telemetry` every 1 s and mirrors buzzer beeps in the browser via Web Audio API. The `buzzer` field in the JSON response is an **array** of up to 8 `BeepEvent` entries (ring buffer, oldest→newest: `seq`, `freq`, `onMs`, `offMs`, `reps`, `active`) — the browser replays all events with `seq > bzLastSeq` in order using a Web Audio time cursor. A 🔔/🔇 toggle button in the status bar unlocks the `AudioContext` (browser autoplay policy) and controls mute.
```

Replace with:

```markdown
The **telemetry page** (`/telemetry`) polls `/api/telemetry` every 1 s and mirrors buzzer sounds in the browser via Web Audio API. The `buzzer` field in the JSON response is an **array** of up to 8 `BeepEvent` entries (ring buffer, oldest→newest: `seq`, `freq`, `onMs`, `offMs`, `reps`, `layer`, `active`) — `layer` is 0 for a queued event, 1 for the persistent state layer. The browser plays fresh queued events (`seq > bzLastSeq`) immediately and toggles a looping oscillator on state transitions only (repeated `active:true`/`active:false` for an already-running/stopped state is ignored); an event pauses the state loop and resumes it afterward. A 🔔/🔇 toggle button in the status bar unlocks the `AudioContext` (browser autoplay policy) and controls mute.
```

- [ ] **Step 5: Update `CLAUDE.md`'s remote-link paragraph**

Find:

```markdown
- **Component:** `src/RemoteLink/` (ESP-NOW transport, beep forwarding, pairing; also holds this repo's copy of `RemoteLinkProtocol.h` — see above). Both buzzers stay active — key beeps are forwarded to the remote via `remoteLink.requestBeep()`.
```

Replace with:

```markdown
- **Component:** `src/RemoteLink/` (ESP-NOW transport, beep forwarding, pairing; also holds this repo's copy of `RemoteLinkProtocol.h` — see above). Both buzzers stay active — key beeps are forwarded to the remote via `remoteLink.requestBeep()`. The remote's `Armed`/`Stop`/`Disarmed` commands now mirror the controller's own armed+stopped state (`updateSoundState()` in `main.cpp`, using the same `throttle.isEngaged()` hysteresis as `Sound`'s `ArmedIdle` state) — the two used to disagree, with the remote beeping on armed alone.
```

- [ ] **Step 6: Update `test/CLAUDE.md`**

Find:

```markdown
- `PowerTest.cpp` - Unit tests for the `Power` class, covering battery limit calculation, motor temperature limiting, combined power limiting, and PWM output mapping.
- `README` - Default PlatformIO README for the test directory.
```

Replace with:

```markdown
- `PowerTest.cpp` - Unit tests for the `Power` class, covering battery limit calculation, motor temperature limiting, combined power limiting, and PWM output mapping.
- `PeriodicTriggerTest.cpp` - Unit tests for `PeriodicTrigger` (`src/Sound/PeriodicTrigger.h`), covering fire-on-entry, periodic re-fire, reset-on-clear, and millis() rollover.
- `SoundLogicTest.cpp` - Unit tests for `SoundLogic` (`src/Sound/SoundLogic.h`), covering the continuous state layer never expiring, event preemption and state resumption, `setState()` idempotency, queued-event ordering, queue-overflow drop policy, and millis() rollover across both layers.
- `README` - Default PlatformIO README for the test directory.
```

- [ ] **Step 7: Commit**

```bash
git add src/CLAUDE.md CLAUDE.md test/CLAUDE.md
git commit -m "docs: document the Sound component and layered audio model"
```

---

### Task 15: On-hardware verification (manual, cannot be automated)

**Files:** none

Host tests prove the policy logic is correct in isolation; they cannot prove the piezo,
the LEDC timer sharing with `ESP32Servo`, or the browser's Web Audio timing behave
correctly on real hardware. Flash `lolin_c3_mini_tmotor` (or `_xag`, whichever the test
rig uses) and verify:

- [ ] Arm with motor stopped -> continuous beep starts; wait **3 minutes** -> still
  beeping (this is the direct regression check for the ~102s bug).
- [ ] During the continuous alert, click the button once -> one click sound, then the
  continuous alert resumes on its own.
- [ ] Five rapid clicks while armed and stopped -> five clicks play in order, then the
  continuous alert resumes.
- [ ] Push the throttle above the engage threshold -> continuous alert stops; return to
  idle -> it resumes.
- [ ] Move the volume slider on the "Sistema" config page while armed and stopped ->
  preview beep plays, continuous alert resumes after.
- [ ] Open `/telemetry`, unmute, and repeat the five checks above -> the browser matches
  the piezo in each case, including on page load while already armed and stopped (should
  start beeping immediately, not stay silent).
- [ ] In wireless throttle mode: power off the remote while armed -> rapid link-loss
  beeps start within ~500ms and repeat every 500ms, then disarm at 3s.
- [ ] Trigger a power alert (e.g. motor temp near the limit) while armed and stopped -> 3
  beeps play, then the continuous alert resumes.

If any check fails, fix the underlying `SoundLogic`/`Sound`/`main.cpp` logic (not by
special-casing in `TelemetryPage.h` alone -- the firmware is the source of truth) and
re-run the full checklist, not just the failing item, since layered-audio bugs tend to
surface as a *different* check failing after a fix.

No commit for this task unless a fix was needed, in which case commit that fix on its
own with a message describing the actual defect found.

---

### Task 16: Remove working documents and open the PR

**Files:**
- Delete: `docs/superpowers/specs/2026-08-02-sound-system-design.md`
- Delete: `docs/superpowers/plans/2026-08-02-sound-system-layers.md`

- [ ] **Step 1: Confirm every host test and both builds are still green**

```bash
c++ -std=c++17 -o /tmp/periodic_test test/PeriodicTriggerTest.cpp && /tmp/periodic_test
c++ -std=c++17 -o /tmp/sound_test test/SoundLogicTest.cpp && /tmp/sound_test
c++ -std=c++17 -o /tmp/power_alert_test test/PowerAlertLogicTest.cpp && /tmp/power_alert_test
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
```

- [ ] **Step 2: Delete the working documents**

```bash
git rm docs/superpowers/specs/2026-08-02-sound-system-design.md docs/superpowers/plans/2026-08-02-sound-system-layers.md
git commit -m "docs: remove working spec/plan before opening the PR"
```

- [ ] **Step 3: Push and open the PR**

```bash
git push -u origin feat/sound-system-layers
```

```bash
gh pr create --title "fix: layered audio system (armed-alert never recovers, self-silences after ~102s)" --body "$(cat <<'EOF'
## Summary
- Replaces the single-slot `Buzzer` with a layered model: a driver-only `Buzzer`, a pure host-tested `SoundLogic` (event queue + persistent state, see `test/SoundLogicTest.cpp`), and a `Sound` component gluing them together.
- Fixes two field-reported bugs by construction rather than by patching the symptom:
  - Any beep (button click, power alert) permanently killed the continuous armed-alert, because it was edge-triggered and had no way to recover once preempted.
  - The armed alert silenced itself after ~102s, because `reps=255` ("continuous") was actually a literal repeat count.
- `isMotorRunning()` now uses the existing `throttle.isEngaged()` hysteresis instead of a raw `>1%` threshold, removing Hall-sensor-noise re-triggering and letting the remote-beep channel finally agree with the controller on when to sound.
- New: an audible warning during the wireless failsafe ramp window (500ms-3s of link loss), where today there is silence.
- The `/telemetry` browser mirror gets a `layer` field (event vs. state) and stops treating `reps===255` as infinite.

## Test plan
- [x] `test/PeriodicTriggerTest.cpp`, `test/SoundLogicTest.cpp` pass (new)
- [x] `test/PowerAlertLogicTest.cpp` passes unchanged after the `PeriodicTrigger` refactor
- [x] `test/RemoteLinkLogicTest.cpp`, `test/ThrottleEngagementLogicTest.cpp`, `test/JkBmsParserTest.cpp` pass unchanged
- [x] `pio run -e lolin_c3_mini_tmotor` and `-e lolin_c3_mini_xag` both build
- [ ] On-hardware checklist from the implementation plan (armed alert survives 3 min, click preemption/resume, telemetry page audio parity, wireless link-loss warning, power alert) -- to be run by whoever has the test rig before merge

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Report the PR URL back once created.
