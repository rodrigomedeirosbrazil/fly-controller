# Buzzer Beep Browser Reliability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix three field-reported bugs: buttons too close together in the status bar, fast beeps lost due to the 1s poll window, and the armed-alert loop never stopping in the browser.

**Architecture:** Replace the single `BeepEvent` snapshot with a ring buffer of the last 8 events in the firmware, serialize them as a JSON array in `/api/telemetry`, and update the browser to replay everything it hasn't seen yet (by monotonic `seq`) — with seq-priming on first load to avoid stale bursts.

**Tech Stack:** C++ / Arduino (ESP32-C3), ArduinoJSON v6, Web Audio API (JavaScript)

---

## File Map

| File | Change |
|------|--------|
| `controller/src/WebServer/Pages/CommonStyles.h` | Add `gap: 10px` to `.tsb-right` |
| `controller/src/Buzzer/Buzzer.h` | Replace `BeepEvent beepEvent_` with ring buffer; new `silence()` / `pushBeepEvent()` / `getBeepEvents()` |
| `controller/src/Buzzer/Buzzer.cpp` | Implement ring buffer, `silence()`, `pushBeepEvent()`, `getBeepEvents()`; update `startBeep()`, `stop()`, `handle()`, `startMelody()` |
| `controller/src/WebServer/ControllerWebServer.cpp` | Serialize buzzer as JSON array; bump `StaticJsonDocument` size |
| `controller/src/WebServer/Pages/TelemetryPage.h` | Replace single-event handler with batch array replay, seq-priming on first poll |

---

## Task 1: CSS — gap between sound and lock buttons

**Files:**
- Modify: `controller/src/WebServer/Pages/CommonStyles.h:305-309`

- [ ] **Step 1: Add `gap` to `.tsb-right`**

  Open `controller/src/WebServer/Pages/CommonStyles.h`. Locate this block (around line 305):

  ```css
  .telemetry-status-bar .tsb-right {
      display: flex;
      align-items: center;
      flex-shrink: 0;
  }
  ```

  Replace it with:

  ```css
  .telemetry-status-bar .tsb-right {
      display: flex;
      align-items: center;
      flex-shrink: 0;
      gap: 10px;
  }
  ```

- [ ] **Step 2: Commit**

  ```bash
  cd /Users/rodrigo/dev/fly-controller/.claude/worktrees/confident-euler-892811
  git add controller/src/WebServer/Pages/CommonStyles.h
  git commit -m "fix: add gap between sound and wake-lock buttons in telemetry status bar"
  ```

---

## Task 2: Firmware — Buzzer ring buffer

**Files:**
- Modify: `controller/src/Buzzer/Buzzer.h`
- Modify: `controller/src/Buzzer/Buzzer.cpp`

The core change: instead of a single `BeepEvent beepEvent_` snapshot, the Buzzer keeps a circular ring of the last 8 events. `startBeep()` pushes an `active:true` event. `stop()` (called externally, e.g. from `main.cpp` when the motor starts) pushes an `active:false` event only if something was playing. Internal transitions (natural end of a finite beep in `handle()`) call the new `silence()` helper, which resets PWM state without logging any event — the browser replays the beep's own `reps` count and needs no stop signal.

- [ ] **Step 1: Rewrite `Buzzer.h`**

  Replace the entire contents of `controller/src/Buzzer/Buzzer.h` with:

  ```cpp
  #ifndef BUZZER_H
  #define BUZZER_H

  #include <Arduino.h>
  #include <driver/ledc.h>

  // Structure for a single note
  struct Note {
    uint16_t frequency;
    uint16_t duration;
  };

  // Structure for a melody (sequence of notes)
  struct Melody {
    const Note* notes;
    uint8_t noteCount;
    uint16_t pauseBetweenNotes;
  };

  // One entry in the beep event ring — for web telemetry transport.
  struct BeepEvent {
    uint32_t seq;       // monotonic counter; 0 = empty slot
    uint16_t frequency; // Hz
    uint16_t onMs;      // on duration
    uint16_t offMs;     // off/pause duration between reps
    uint8_t  reps;      // 255 = continuous
    bool     active;    // true = started, false = stopped
  };

  class Buzzer {
  public:
    static constexpr uint8_t kRingSize = 8;

  private:
    uint8_t pin;
    uint8_t pwmChannel;
    uint32_t pwmFrequency;
    uint8_t pwmResolution;
    uint32_t pwmDutyCycle;
    bool playing;
    uint32_t startTime;
    uint16_t beepDuration;
    uint16_t pauseDuration;
    uint8_t repetitions;
    uint8_t currentRepetition;
    bool isOn;

    // Melody/sequence support
    bool playingMelody;
    const Melody* currentMelody;
    uint8_t currentNoteIndex;
    uint32_t noteStartTime;
    bool noteIsOn;
    bool melodyRepeat;

    // Event ring buffer
    BeepEvent beepRing_[kRingSize];
    uint8_t beepWriteIdx_;
    uint8_t beepCount_;
    uint32_t beepSeq_;

    void startBeep(uint16_t duration, uint8_t reps, uint16_t pause, uint16_t frequency = 0);
    void startMelody(const Melody* melody, bool repeat = false);
    bool isPlaying() { return playing || playingMelody; }
    void setPwmOn();
    void setPwmOff();
    void setFrequency(uint16_t frequency);
    // Stops PWM and resets playback state without logging an event.
    void silence();
    // Appends one event to the ring (seq auto-assigned).
    void pushBeepEvent(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t reps, bool active);

  public:
    Buzzer(uint8_t buzzerPin);
    void setup();
    void handle();

    // Re-applies the timer frequency after the global LEDC clock source
    // changes (e.g. when ESP32Servo's esc.attach() forces XTAL on the
    // shared low-speed clock, halving previously-configured frequencies).
    void recalibrate();

    // Sets the output volume as a percentage (0-100), mapped directly to the
    // PWM duty cycle. 0 = silent. Takes effect on the next beep.
    void setVolume(uint8_t percent);

    // Contextual methods
    void beepSystemStart();
    void beepCalibrationStep();
    void beepCalibrationComplete();
    void beepDisarmed();
    void beepArmingBlocked();
    void beepButtonClick();
    void beepArmedAlert();
    void beepVolumePreview();

    // Stops playback and logs an active:false event if something was playing.
    void stop();

    // Returns events in ascending seq order (oldest first), up to maxCount.
    // Returns the number written into buf.
    uint8_t getBeepEvents(BeepEvent* buf, uint8_t maxCount) const;
  };

  #endif
  ```

- [ ] **Step 2: Update `Buzzer.cpp` — constructor**

  Open `controller/src/Buzzer/Buzzer.cpp`. Replace the constructor (lines 19-39) with:

  ```cpp
  Buzzer::Buzzer(uint8_t buzzerPin) :
    pin(buzzerPin),
    pwmChannel(1),
    pwmFrequency(kDefaultBeepFrequencyHz),
    pwmResolution(8),
    pwmDutyCycle(kDefaultDutyCycle),
    playing(false),
    startTime(0),
    beepDuration(0),
    pauseDuration(0),
    repetitions(0),
    currentRepetition(0),
    isOn(false),
    playingMelody(false),
    currentMelody(nullptr),
    currentNoteIndex(0),
    noteStartTime(0),
    noteIsOn(false),
    melodyRepeat(false),
    beepRing_{},
    beepWriteIdx_(0),
    beepCount_(0),
    beepSeq_(0) {
  }
  ```

- [ ] **Step 3: Add `silence()` and `pushBeepEvent()` after `setup()` in Buzzer.cpp**

  After the closing `}` of `Buzzer::setup()` (around line 62), add these two new methods:

  ```cpp
  void Buzzer::silence() {
    playing = false;
    playingMelody = false;
    isOn = false;
    noteIsOn = false;
    repetitions = 0;
    currentMelody = nullptr;
    currentNoteIndex = 0;
    melodyRepeat = false;
    setPwmOff();
  }

  void Buzzer::pushBeepEvent(uint16_t freq, uint16_t onMs, uint16_t offMs, uint8_t reps, bool active) {
    beepRing_[beepWriteIdx_] = { ++beepSeq_, freq, onMs, offMs, reps, active };
    beepWriteIdx_ = (beepWriteIdx_ + 1) % kRingSize;
    if (beepCount_ < kRingSize) beepCount_++;
  }
  ```

- [ ] **Step 4: Add `getBeepEvents()` after `pushBeepEvent()` in Buzzer.cpp**

  ```cpp
  uint8_t Buzzer::getBeepEvents(BeepEvent* buf, uint8_t maxCount) const {
    uint8_t count = (beepCount_ < maxCount) ? beepCount_ : maxCount;
    uint8_t start = (beepWriteIdx_ + kRingSize - beepCount_) % kRingSize;
    for (uint8_t i = 0; i < count; i++) {
      buf[i] = beepRing_[(start + i) % kRingSize];
    }
    return count;
  }
  ```

- [ ] **Step 5: Update `handle()` — replace internal `stop()` calls with `silence()`**

  In `handle()`, locate the two internal `stop()` calls and replace them:

  1. When a finite beep finishes all reps (around line 131-135):
     ```cpp
     if (++currentRepetition >= repetitions) {
         silence();  // was stop()
         return;
     }
     ```

  2. When a non-repeating melody finishes (around line 104-108):
     ```cpp
     } else {
         silence();  // was stop()
         return;
     }
     ```

  Do NOT change `return;` statements or any other logic — only the `stop()` → `silence()` calls.

- [ ] **Step 6: Update `startBeep()` — use `silence()` and push event**

  Locate `Buzzer::startBeep()` (around line 146). Replace the entire function body with:

  ```cpp
  void Buzzer::startBeep(uint16_t duration, uint8_t reps, uint16_t pause, uint16_t frequency) {
    if (playing || playingMelody) {
      silence();
    }

    if (frequency > 0) {
      setFrequency(frequency);
    }

    beepDuration = duration;
    pauseDuration = pause;
    repetitions = reps;
    currentRepetition = 0;
    playing = true;
    isOn = true;
    setPwmOn();
    startTime = millis();
    pushBeepEvent(pwmFrequency, duration, pause, reps, true);
  }
  ```

- [ ] **Step 7: Update `startMelody()` — use `silence()`**

  In `startMelody()` (around line 171), replace `stop()` with `silence()`:

  ```cpp
  void Buzzer::startMelody(const Melody* melody, bool repeat) {
    if (playing || playingMelody) {
      silence();  // was stop()
    }
    // ... rest of function unchanged
  ```

- [ ] **Step 8: Rewrite `stop()`**

  Replace the entire `stop()` function (around line 257-268) with:

  ```cpp
  void Buzzer::stop() {
    bool wasPlaying = playing || playingMelody;
    silence();
    if (wasPlaying) {
      pushBeepEvent(pwmFrequency, 0, 0, 0, false);
    }
  }
  ```

- [ ] **Step 9: Verify the file compiles — check for leftover references to the old API**

  Run a grep to make sure `beepEvent_` and `getBeepEvent()` (old names) are gone:

  ```bash
  cd /Users/rodrigo/dev/fly-controller/.claude/worktrees/confident-euler-892811
  grep -rn "beepEvent_\|getBeepEvent\b" controller/src/
  ```

  Expected: **no output**. If any matches appear, fix them before continuing.

- [ ] **Step 10: Commit**

  ```bash
  cd /Users/rodrigo/dev/fly-controller/.claude/worktrees/confident-euler-892811
  git add controller/src/Buzzer/Buzzer.h controller/src/Buzzer/Buzzer.cpp
  git commit -m "feat: replace single BeepEvent snapshot with ring buffer of 8 events"
  ```

---

## Task 3: API — serialize buzzer ring as JSON array

**Files:**
- Modify: `controller/src/WebServer/ControllerWebServer.cpp:653,718-727`

- [ ] **Step 1: Bump `StaticJsonDocument` capacity**

  On line 653, change:

  ```cpp
  StaticJsonDocument<896> doc;
  ```

  to:

  ```cpp
  StaticJsonDocument<2048> doc;
  ```

  Rationale: the buzzer array now holds up to 8 objects (6 fields each). ArduinoJSON slot overhead ≈ JSON_ARRAY_SIZE(8) + 8·JSON_OBJECT_SIZE(6) ≈ 128 + 768 = 896 extra bytes over the old single object (~96 bytes), so +800 bytes. 2048 gives comfortable headroom for the total document.

- [ ] **Step 2: Replace the buzzer serialization block**

  Locate this block (lines 718-727):

  ```cpp
  {
      const BeepEvent ev = buzzer.getBeepEvent();
      JsonObject buzzerObj = doc.createNestedObject("buzzer");
      buzzerObj["seq"]    = ev.seq;
      buzzerObj["freq"]   = ev.frequency;
      buzzerObj["onMs"]   = ev.onMs;
      buzzerObj["offMs"]  = ev.offMs;
      buzzerObj["reps"]   = ev.reps;
      buzzerObj["active"] = ev.active;
  }
  ```

  Replace it with:

  ```cpp
  {
      BeepEvent evBuf[Buzzer::kRingSize];
      const uint8_t evCount = buzzer.getBeepEvents(evBuf, Buzzer::kRingSize);
      JsonArray buzzerArr = doc.createNestedArray("buzzer");
      for (uint8_t i = 0; i < evCount; i++) {
          JsonObject ev = buzzerArr.createNestedObject();
          ev["seq"]    = evBuf[i].seq;
          ev["freq"]   = evBuf[i].frequency;
          ev["onMs"]   = evBuf[i].onMs;
          ev["offMs"]  = evBuf[i].offMs;
          ev["reps"]   = evBuf[i].reps;
          ev["active"] = evBuf[i].active;
      }
  }
  ```

- [ ] **Step 3: Build to verify no compilation errors**

  ```bash
  cd /Users/rodrigo/dev/fly-controller/controller
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | tail -5
  ```

  Expected: `SUCCESS`

- [ ] **Step 4: Commit**

  ```bash
  cd /Users/rodrigo/dev/fly-controller/.claude/worktrees/confident-euler-892811
  git add controller/src/WebServer/ControllerWebServer.cpp
  git commit -m "feat: serialize buzzer event ring as JSON array in /api/telemetry"
  ```

---

## Task 4: Browser — batch event replay with seq priming

**Files:**
- Modify: `controller/src/WebServer/Pages/TelemetryPage.h`

Key behavioral changes:
- **Priming:** On the first successful `/api/telemetry` response, `bzLastSeq` is set to the highest seq in the ring without playing anything. This prevents stale beeps from replaying when the page opens mid-session.
- **Batch replay:** On every subsequent poll, events with `seq > bzLastSeq` are played in order. One-shots are scheduled back-to-back using an audio-time cursor so they don't overlap.
- **Stop event:** An `active:false` event now carries a new `seq`, so `bzLastSeq` advances and the browser calls `bzStopLoop()` — fixing the stuck armed-alert bug.

- [ ] **Step 1: Add `bzPrimed` variable and remove `bzNextAction`**

  Find the variable declarations block (lines 140-149):

  ```javascript
  let bzLoopActive = false;
  let bzStopLoopFlag = false;
  let bzActiveOsc = null;
  let bzActiveGain = null;
  let bzLastSeq = -1;
  let bzLoopIsRunning = false;
  ```

  Replace with:

  ```javascript
  let bzLoopActive = false;
  let bzStopLoopFlag = false;
  let bzActiveOsc = null;
  let bzActiveGain = null;
  let bzLastSeq = -1;
  let bzPrimed = false;
  let bzLoopIsRunning = false;
  ```

  Then delete the `bzNextAction` function entirely (lines 233-239):

  ```javascript
  // Pure function — easy to test in isolation
  const bzNextAction = (prevSeq, curSeq, reps, active) => {
      if (curSeq === prevSeq) return 'none';
      if (!active)            return 'stop';
      if (reps === 255)       return 'start-loop';
      return 'play-once';
  };
  ```

- [ ] **Step 2: Replace `bzPlayOnce` with `bzScheduleOnce`**

  Delete `bzPlayOnce` (lines 184-201):

  ```javascript
  const bzPlayOnce = (freq, onMs, offMs, reps) => {
      if (!audioCtx) return;
      bzStopLoop();
      let t = audioCtx.currentTime;
      for (let i = 0; i < reps; i++) {
          ...
      }
  };
  ```

  Add in its place `bzScheduleOnce`, which takes a pre-computed start time and returns the new cursor:

  ```javascript
  // Schedules reps on/off pulses starting at startT; returns the time cursor after the last pulse.
  const bzScheduleOnce = (freq, onMs, offMs, reps, startT) => {
      let t = startT;
      for (let i = 0; i < reps; i++) {
          const osc = audioCtx.createOscillator();
          const g = audioCtx.createGain();
          osc.connect(g);
          g.connect(gainMaster);
          osc.type = 'square';
          osc.frequency.value = freq;
          g.gain.setValueAtTime(1, t);
          g.gain.setValueAtTime(0, t + onMs / 1000);
          osc.start(t);
          osc.stop(t + onMs / 1000);
          t += (onMs + offMs) / 1000;
      }
      return t;
  };
  ```

- [ ] **Step 3: Replace `bzProcessEvent` with `bzProcessEvents`**

  Delete the old `bzProcessEvent` (lines 241-253):

  ```javascript
  const bzProcessEvent = (bz) => {
      if (!bz) return;
      const action = bzNextAction(bzLastSeq, bz.seq, bz.reps, bz.active);
      if (action === 'none') return;
      bzLastSeq = bz.seq;
      ...
  };
  ```

  Add in its place:

  ```javascript
  // Plays a batch of events in order using an audio-time cursor for one-shots.
  // Events must be pre-filtered (seq > bzLastSeq) and in ascending seq order.
  const bzPlayQueue = (events) => {
      if (!audioCtx) return;
      let t = audioCtx.currentTime;
      for (const ev of events) {
          if (!ev.active) {
              bzStopLoop();
              t = audioCtx.currentTime;
          } else if (ev.reps === 255) {
              // bzStartLoop calls bzStopLoop internally
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

  // Called on every telemetry poll with the buzzer array from the API.
  // First call primes bzLastSeq without playing (avoids stale bursts on page open).
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

- [ ] **Step 4: Update the call site in `renderTelemetry`**

  On line 676, replace:

  ```javascript
  bzProcessEvent(data.buzzer);
  ```

  with:

  ```javascript
  bzProcessEvents(data.buzzer);
  ```

- [ ] **Step 5: Verify no references to deleted functions remain**

  ```bash
  cd /Users/rodrigo/dev/fly-controller/.claude/worktrees/confident-euler-892811
  grep -n "bzProcessEvent\b\|bzNextAction\b\|bzPlayOnce\b" controller/src/WebServer/Pages/TelemetryPage.h
  ```

  Expected: **no output** (the `bzProcessEvents` call on line 676 does NOT match `bzProcessEvent\b`).

- [ ] **Step 6: Commit**

  ```bash
  cd /Users/rodrigo/dev/fly-controller/.claude/worktrees/confident-euler-892811
  git add controller/src/WebServer/Pages/TelemetryPage.h
  git commit -m "feat: batch buzzer event replay with seq-priming in browser telemetry page"
  ```

---

## Task 5: Build verification

- [ ] **Step 1: Build all four targets**

  Run from `controller/`:

  ```bash
  cd /Users/rodrigo/dev/fly-controller/controller
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_hobbywing 2>&1 | grep -E "SUCCESS|ERROR|error:"
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_tmotor 2>&1 | grep -E "SUCCESS|ERROR|error:"
  ~/.platformio/penv/bin/pio run -e lolin_c3_mini_xag 2>&1 | grep -E "SUCCESS|ERROR|error:"
  ```

  ```bash
  cd /Users/rodrigo/dev/fly-controller/throttle
  ~/.platformio/penv/bin/pio run -e remote_throttle 2>&1 | grep -E "SUCCESS|ERROR|error:"
  ```

  Expected: four `SUCCESS` lines.

- [ ] **Step 2: Verify no `doc.overflowed()` path is hit at runtime (optional manual check)**

  After flashing, open `/api/telemetry` in a browser. Confirm the `buzzer` field is an array (e.g., `"buzzer":[]` or `"buzzer":[{...}]`), not a 500 error.
