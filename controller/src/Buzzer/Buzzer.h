#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include <driver/ledc.h>

// Structure for a single note
struct Note {
  uint16_t frequency;  // Frequency in Hz
  uint16_t duration;  // Duration in ms
};

// Structure for a melody (sequence of notes)
struct Melody {
  const Note* notes;
  uint8_t noteCount;
  uint16_t pauseBetweenNotes;  // Pause between notes in ms
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
  void beepPowerAlert();

  // Stops playback and logs an active:false event if something was playing.
  void stop();

  // Returns events in ascending seq order (oldest first), up to maxCount.
  // Returns the number written into buf.
  uint8_t getBeepEvents(BeepEvent* buf, uint8_t maxCount) const;
};

#endif
