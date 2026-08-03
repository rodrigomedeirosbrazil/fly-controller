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
