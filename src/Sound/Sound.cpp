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
