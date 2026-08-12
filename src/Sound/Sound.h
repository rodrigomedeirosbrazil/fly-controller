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

  // Requests the state layer's frequency for its next on-phase (the variable
  // arm-charge / disarm-ramp tones). See SoundLogic::setStateFreq.
  void setStateFreq(uint16_t freqHz);

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
