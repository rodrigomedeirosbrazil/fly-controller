# Sound System Redesign — Layered Audio (Events + State)

**Date:** 2026-08-02
**Branch:** `feat/sound-system-layers` (from `main`)
**Status:** Approved design, pending implementation plan

> **WORKING DOCUMENT — REMOVE BEFORE OPENING THE PR.**
> `docs/superpowers/` is gitignored; this file was committed with `git add -f` so the
> design travels with the branch during implementation. It MUST be deleted in a commit
> before the pull request is opened.

## Problem

Two field-reported bugs, both traced to a single root cause.

### Bug 1 — any beep kills the armed alert permanently

`Buzzer` has a single playback slot. `startBeep()` calls `silence()` before doing
anything else (`src/Buzzer/Buzzer.cpp:177`), so every new sound cancels whatever was
playing. The continuous armed alert is a victim of this, and it never comes back because
its only trigger is a state *edge* (`src/main.cpp:252`):

```cpp
if (isArmed && !motorRunning && (!wasArmed || wasMotorRunning)) {
    buzzer.beepArmedAlert();
}
```

Click the button while armed and stopped → `beepButtonClick()` → `silence()` kills the
alert → no edge ever occurs again → the safety alert is gone until the pilot disarms and
rearms, or spins the motor and stops it. `beepPowerAlert()` (every 10 s while a limiter
is active) has the same side effect.

### Bug 2 — the armed alert dies on its own after ~102 seconds

`beepArmedAlert()` is `startBeep(200, 255, 200)`. The `255` is intended as "continuous"
but the playback engine treats it as a literal repetition count
(`src/Buzzer/Buzzer.cpp:161`):

```cpp
if (++currentRepetition >= repetitions) { silence(); return; }
```

255 x (200 ms on + 200 ms off) = **102 seconds**, then the piezo goes quiet. The
`BeepEvent` struct documents `reps 255 = continuous` and the browser mirror
(`src/WebServer/Pages/TelemetryPage.h:248`) *does* treat 255 as a true infinite loop —
so the web page keeps beeping after the device has already stopped. The two
implementations disagree.

### Related findings

- **Logical duplication causes an erratic pattern.** `isMotorRunning()`
  (`src/main.cpp:240`) uses `throttle.getThrottlePercentage() > 1` with no hysteresis.
  Hall-sensor noise around 1% flips `motorRunning`, each flip satisfies the edge
  condition, and `beepArmedAlert()` restarts the cycle from zero. The result is an
  irregular/stuttering alert. Note that *acoustic* overlap is impossible on this
  hardware (one LEDC channel, one timer, one piezo) — the problem is re-triggering.
- **Link loss is silent.** In wireless mode the failsafe ramps throttle to zero between
  500 ms and 3 s of packet loss with no audible warning (`src/main.cpp:159` only handles
  the `Disarm` case). The pilot hears nothing until the disarm beep at 3 s.
- **Controller and remote disagree on when to beep.** The remote beeps on *armed*
  (`src/main.cpp:262`), the controller on *armed + motor stopped*. The code comment
  explains the decoupling was to avoid `Stop`/`Armed` oscillation from throttle noise
  crossing the `isMotorRunning` threshold.
- **Dead code.** `Melody` / `Note` / `startMelody` exist but `startMelody` is private and
  never called.

**Root cause:** there is no layering model. Continuous state sounds and momentary event
sounds compete for one slot, and consumers are edge-triggered, so a preempted state
sound has no way to recover.

## Decisions Taken

| # | Question | Decision |
|---|---|---|
| 1 | Momentary beep during the continuous alert | **Queue** for events + a state layer recalculated every loop, so it never depends on an edge to recover |
| 2 | Scope of the remote | **Controller now, remote later.** No protocol change; fix the rule inconsistency in `main.cpp` only |
| 3 | Which sounds are "state" | **Only armed+stopped.** Power alert stays a periodic event; link loss becomes a new periodic event |
| 4 | Browser mirror | **Simplified mirror.** Firmware announces the layer explicitly; the browser plays queue events and toggles the state sound, accepting a few ms of timing imprecision |

## Architecture

Three layers, each with one responsibility and independently testable.

### `Buzzer/` — hardware driver only

Shrinks from 293 lines to roughly 70. Complete API:

```cpp
void setup();
void recalibrate();            // unchanged: re-applies freq after ESP32Servo clock change
void setVolume(uint8_t pct);   // duty cycle — hardware concern, stays here
void toneOn(uint16_t freqHz);
void toneOff();
```

Removed: repetition counting, `startBeep`, all nine `beepXxx()` methods, the `BeepEvent`
ring, and the dead `Melody`/`Note`/`startMelody` code. `Buzzer` loses all notion of time
— it no longer has a `handle()`.

Stays on LEDC timer 1 / channel 1, and keeps `recalibrate()`. That boundary with
ESP32Servo is delicate and is not touched.

### `Sound/SoundLogic.h` — pure policy, host-testable

No `Arduino.h`, no `driver/ledc.h`. Follows the existing pattern of
`PowerAlertLogic.h`, `RemoteLinkLogic.h`, and `ThrottleEngagementLogic`.

```cpp
struct SoundPattern {
    uint16_t freqHz;
    uint16_t onMs;
    uint16_t offMs;
    uint8_t  reps;    // 0 = continuous (replaces the 255 hack)
};

// Two separate enums: overlap becomes a compile-time impossibility.
enum class SoundEvent : uint8_t {   // queue only, always finite
    SystemStart, CalibrationStep, CalibrationComplete, Disarmed,
    ArmingBlocked, ButtonClick, VolumePreview, PowerAlert, LinkLoss,
};

enum class SoundState : uint8_t {   // state layer only
    None = 0,
    ArmedIdle,
};

struct SoundOutput { bool toneOn; uint16_t freqHz; };
```

Two inputs, one output:

- `pushEvent(SoundEvent)` — enqueues a momentary sound (fixed ring of 4).
- `setState(SoundState)` — declares the desired persistent sound. **Idempotent**:
  calling it with the same value every loop does not restart the cycle; only a
  transition has an effect.
- `update(nowMs) -> SoundOutput` — called every tick; returns whether the tone should be
  on and at which frequency.

Resolution rules, all inside this class:

1. Queue non-empty → the front event plays; the state sound is **preempted
   immediately** (cut mid-tone, no waiting).
2. Queue drained and a state is active → the state **restarts from the beginning of its
   cycle**.
3. `reps == 0` is genuinely continuous — no counter can reach an end. This fixes Bug 2.
4. Queue full → drop the **newest** and increment `droppedEvents()`. Dropping the newest
   (rather than the oldest) avoids truncating a sequence already in flight. With a ring
   of 4 and events of 50–250 ms, overflow requires ~4 gestures in under half a second.
5. No duplicate coalescing: two real clicks are two beeps. Predictability over
   cleverness.
6. Time comparisons use the `gap > 0x80000000UL` guard from `RemoteLinkLogic.h` to
   survive the `millis()` rollover at 49 days.

**Why two enums.** `play(ArmedIdle)` must not compile. The armed alert cannot enter the
queue and cannot exist in two instances — not even by a future mistake. The state layer
is a single variable (`SoundState stateId_`), not a queue: `setState(ArmedIdle)` called
400 times per second writes the same value 400 times and `update()` sees no transition,
so the pattern cycle continues where it was. No structure exists that could hold two
instances of the same state.

### `Sound/PeriodicTrigger.h` — pure, host-testable

Fires on entry into a condition, then every `intervalMs` while it holds, resets when the
condition clears. Used for the link-loss warning.

This is literally the shape of the existing `PowerAlertLogic`. `PowerAlertLogic` will be
reimplemented as a thin wrapper over `PeriodicTrigger`, keeping
`test/PowerAlertLogicTest.cpp` passing unchanged. `PowerAlert.cpp` is being modified
anyway; leaving two near-identical classes would be worse.

### `Sound/Sound.{h,cpp}` — the component

Thin glue: owns the `SoundPattern` catalog (today's empirically tuned values — 2300 Hz
general, 2000 Hz armed alert, 2500 Hz power alert), translates `SoundOutput` into
`buzzer.toneOn()`/`toneOff()`, and maintains the `BeepEvent` ring the web server
consumes.

```cpp
void play(SoundEvent);
void setState(SoundState);
void handle();
uint8_t getBeepEvents(BeepEvent* buf, uint8_t maxCount) const;
```

Instantiated in `config.cpp` with `extern Sound sound;` in `config.h`. It needs no
`setup()` — `buzzer.setup()` in `main.cpp::setup()` still handles LEDC init.

**`BeepEvent` moves from `Buzzer.h` to `Sound.h`** and gains a `layer` field, since
`Sound` now owns the ring. `Buzzer::kRingSize` becomes `Sound::kRingSize`; the reference
in `ControllerWebServer.cpp:786` is updated accordingly.

One hardware detail the separation makes explicit: changing frequency while the tone is
on produces an audible click on the piezo. `Sound` always calls `toneOff()` before
`toneOn(newFreq)` on a pattern change — today this happens by accident inside
`startBeep`, and becomes a guarantee.

## Data Flow and Call Sites

### `loop()` order

`sound.handle()` takes the place of `buzzer.handle()` on the first line, preserving the
reason documented today (silence a tone that finished before any slow component runs).
Internally: `logic_.update(millis())` → `buzzer.toneOn/toneOff` → publish to the web ring
if a transition occurred.

### `handleArmedBeep()` becomes declarative, renamed `updateSoundState()`

```cpp
void updateSoundState()
{
    bool armedIdle = throttle.isArmed() && !isMotorRunning();
    sound.setState(armedIdle ? SoundState::ArmedIdle : SoundState::None);
    // ...remote channel, below
}
```

Two `static bool` variables disappear. There is no edge left to miss, so Bug 1 becomes
impossible: even if a click preempts the tone, on the next tick the desired state is
still `ArmedIdle` and `SoundLogic` resumes on its own.

### `isMotorRunning()` uses the existing hysteresis

```cpp
bool isMotorRunning() {
    return throttle.isArmed() && throttle.isEngaged();
}
```

`throttle.isEngaged()` is the 2%/1% hysteresis gate (`ThrottleEngagementLogic`, already
host-tested) built precisely to shield against Hall drift/noise at idle. Using the raw
`> 1%` here while `Power` already uses `isEngaged()` is an inconsistency: the sound and
the motor disagree on what "motor running" means. Unifying both kills the oscillation at
its source.

### Remote channel becomes consistent with the controller

The comment at `src/main.cpp:260` explains the remote was decoupled from motor state to
avoid `Stop`/`Armed` oscillation from throttle noise crossing the `isMotorRunning`
threshold. With `isMotorRunning()` now using `throttle.isEngaged()`, **that reason no
longer exists**. So both agree: `RemoteBeep::Armed` on entering armed+stopped,
`RemoteBeep::Stop` on leaving it, `Disarmed` on disarm. No protocol change — those
commands already exist.

### Call-site renames

| File | Before | After |
|---|---|---|
| `src/main.cpp:129` | `buzzer.beepSystemStart()` | `sound.play(SoundEvent::SystemStart)` |
| `src/Button/Button.cpp:58` | `buzzer.beepButtonClick()` | `sound.play(SoundEvent::ButtonClick)` |
| `src/Throttle/Throttle.cpp:84` | `beepCalibrationStep()` | `SoundEvent::CalibrationStep` |
| `src/Throttle/Throttle.cpp:127` | `beepCalibrationComplete()` | `SoundEvent::CalibrationComplete` |
| `src/Throttle/Throttle.cpp:198,203` | `beepArmingBlocked()` | `SoundEvent::ArmingBlocked` |
| `src/Throttle/Throttle.cpp:220` | `beepDisarmed()` | `SoundEvent::Disarmed` |
| `src/PowerAlert/PowerAlert.cpp:21` | `beepPowerAlert()` | `SoundEvent::PowerAlert` |
| `src/WebServer/ControllerWebServer.cpp:296` | `beepVolumePreview()` | `SoundEvent::VolumePreview` |

`buzzer.setVolume()` does **not** move — volume is duty cycle, i.e. hardware. The web
server keeps calling `buzzer.setVolume()` and then `sound.play(VolumePreview)`.

### New: audible link-loss warning

In the failsafe block at `src/main.cpp:159`, which today handles only the `Disarm` case:

```cpp
FailsafeAction action = remoteLink.failsafe(true, millis());
if (linkLossTrigger_.update(action == FailsafeAction::RampToZero,
                            millis(), SOUND_LINK_LOSS_INTERVAL_MS)) {
    sound.play(SoundEvent::LinkLoss);
}
if (action == FailsafeAction::Disarm) throttle.setDisarmed();
```

`SOUND_LINK_LOSS_INTERVAL_MS` is 500 ms, with a short high-pitched pattern distinct from
everything else. The ramp window runs from 500 ms to 3 s, so the pilot hears about five
rapid warnings before disarm — real time to react, instead of today's silence.

**Known interaction, accepted deliberately:** during link loss the throttle is fed 0, so
`isEngaged()` is false and `ArmedIdle` is also active. The continuous armed tone will be
interrupted every 500 ms by the link warning. This is desirable — it sounds
unambiguously like "something is wrong", and the fast high pattern dominates perception.
Suppressing `ArmedIdle` during link loss would require a second state value and would
contradict decision 3. Reversible if field testing disagrees.

### `/api/telemetry` contract

```json
"buzzer": [
  {"seq":12,"freq":2300,"onMs":50,"offMs":0,"reps":1,"layer":0,"active":true},
  {"seq":13,"freq":2000,"onMs":200,"offMs":200,"reps":0,"layer":1,"active":true},
  {"seq":14,"freq":0,"onMs":0,"offMs":0,"reps":0,"layer":1,"active":false}
]
```

`layer`: 0 = event, 1 = state. `reps: 0` = continuous — the `255` hack is removed from
both sides. The state layer publishes an event only on **transition** (one start, one
stop), never repeatedly.

### Browser mirror (`TelemetryPage.h`)

- The `reps === 255` special case is removed.
- `bzStateActive` holds `{freq, onMs, offMs}` of the current state sound.
- `layer:1 active:true` for an already-running state is **ignored** (does not restart the
  loop, so no duplicated `bzStartLoop()`).
- `layer:1 active:false` stops the state loop.
- `layer:0` goes to the queue; if a state loop is running it stops, the event plays, and
  the state resumes at the end of the event.
- **Priming change:** today the first poll only records the highest `seq` and plays
  nothing, to avoid replaying history. With a state layer that becomes wrong — opening
  the page while armed and stopped would leave the mirror silent while the piezo beeps.
  Priming now skips stale queue events but **applies** the last state event, so the page
  starts synchronized.

## Testing

### `test/SoundLogicTest.cpp`

| Test | Guards against |
|---|---|
| Continuous state driven across 10 simulated minutes, including the 102 000 ms mark | **Bug 2** — the exact regression that occurred |
| Event preempts state; state resumes when the event ends | **Bug 1** |
| `setState` with the same value 1000x does not restart the cycle | Logical duplication |
| `setState(None)` silences immediately | — |
| 4 queued events play in order, no overlap, correct total duration | Clicks trampling each other |
| Queue full drops the newest, `droppedEvents()` increments, in-flight sequence intact | Rule 4 |
| `nowMs` crossing the 2^32 rollover mid-cycle and mid-event | Lockup after 49 days powered |
| Every frequency change is preceded by `toneOff` | Audible piezo click |

### `test/PeriodicTriggerTest.cpp`

Fires on entry, re-fires on interval, resets when the condition clears, survives
rollover.

### Commands

```bash
c++ -std=c++17 -o /tmp/sound_test test/SoundLogicTest.cpp && /tmp/sound_test
c++ -std=c++17 -o /tmp/periodic_test test/PeriodicTriggerTest.cpp && /tmp/periodic_test
c++ -std=c++17 -o /tmp/power_alert_test test/PowerAlertLogicTest.cpp && /tmp/power_alert_test
~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor
~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag
```

### On-hardware verification

Host tests cannot prove any of this. Checklist for the end:

1. Arm with motor stopped → continuous beep; wait **3 minutes** → still beeping.
2. During the continuous alert, click the button → one click, then the continuous
   returns.
3. Five rapid clicks → five clicks in order, then the continuous returns.
4. Throttle above the engage threshold → continuous stops; back to idle → returns.
5. Move the volume slider while armed and stopped → preview plays, continuous returns.
6. `/telemetry` page with sound enabled → the browser matches the piezo in all cases
   above.
7. Wireless: power off the remote while armed → rapid link-loss beeps, then disarm.
8. Power alert while armed and stopped → 3 beeps, continuous returns.

## Risks

**Changing `isMotorRunning()` affects the hour meter.** `src/main.cpp:167` calls
`hourMeter.handle(throttle.isArmed(), isMotorRunning())`. Changing the criterion changes
what counts as motor time: today any noise above 1% counts, afterwards it requires the
2% gate with hysteresis. In practice the hours become *more* correct (idle noise stops
accumulating), but this is a behavior change outside the sound subsystem and must be a
conscious one. If undesired, keep a separate predicate for the hour meter.

**Heap on the tmotor build.** That target is already tight (commit 61fa202 addressed
exactly this). No dynamic allocation: the 4-event queue and the 8-entry `BeepEvent` ring
are fixed arrays, as today. `Buzzer` shrinks more than `Sound` grows, so the flash delta
should be slightly negative.

**Shared LEDC with ESP32Servo.** `Buzzer` stays on timer 1 / channel 1 and keeps
`recalibrate()`. That boundary is unchanged.

## Out of Scope

- **Remote firmware** (decision 2). `fly-throttle` keeps mapping `RemoteBeep` bytes to
  its own patterns. A future alignment would mean sending *state* instead of a beep
  command, with a `REMOTE_LINK_PROTOCOL_VERSION` bump and coordinated changes in both
  repos.
- **Power alert** stays a 10 s periodic event (decision 3).
- **No new NVS settings or web UI**: no per-sound volume, no persistent mute.
- **Melodies do not return.** `Melody`/`Note`/`startMelody` are removed as dead code.

## Files Touched

**New:** `src/Sound/SoundLogic.h`, `src/Sound/PeriodicTrigger.h`,
`src/Sound/Sound.{h,cpp}`, `test/SoundLogicTest.cpp`, `test/PeriodicTriggerTest.cpp`

**Modified:** `src/Buzzer/Buzzer.{h,cpp}`, `src/config.{h,cpp}`, `src/main.cpp`,
`src/Button/Button.cpp`, `src/Throttle/Throttle.cpp`,
`src/PowerAlert/PowerAlert.{h,cpp}`, `src/PowerAlert/PowerAlertLogic.h`,
`src/WebServer/ControllerWebServer.cpp`, `src/WebServer/Pages/TelemetryPage.h`,
`src/CLAUDE.md`, `CLAUDE.md`, `test/CLAUDE.md`
