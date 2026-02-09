#include "Buzzer.h"

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),       // Use channel 1 to avoid conflict with ESP32Servo (uses timer 0)
  pwmFrequency(2500),  // 2.5 kHz - typical frequency for passive buzzers
  pwmResolution(8),    // 8-bit resolution (0-255)
  pwmDutyCycle(255),   // 100% duty cycle for maximum volume
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
  melodyRepeat(false) {
}

void Buzzer::setup() {
  ledc_timer_config_t ledc_timer;
  ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
  ledc_timer.timer_num = LEDC_TIMER_1;  // Changed from LEDC_TIMER_0 to avoid ESP32Servo conflict
  ledc_timer.freq_hz = pwmFrequency;
  ledc_timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&ledc_timer);

  ledc_channel_config_t ledc_channel;
  ledc_channel.gpio_num = (gpio_num_t)pin;
  ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
  ledc_channel.channel = (ledc_channel_t)pwmChannel;
  ledc_channel.timer_sel = LEDC_TIMER_1;  // Changed from LEDC_TIMER_0 to avoid ESP32Servo conflict
  ledc_channel.intr_type = LEDC_INTR_DISABLE;
  ledc_channel.duty = 0;
  ledc_channel.hpoint = 0;
  ledc_channel_config(&ledc_channel);

  setPwmOff();
}

void Buzzer::handle() {
  uint32_t currentTime = millis();

  // Handle melody/sequence playback
  if (playingMelody && currentMelody) {
    if (noteIsOn) {
      // Currently playing a note
      uint32_t elapsed = currentTime - noteStartTime;
      if (elapsed >= currentMelody->notes[currentNoteIndex].duration) {
        // Note finished, turn off
        setPwmOff();
        noteIsOn = false;
        noteStartTime = currentTime;
        currentNoteIndex++; // Move to next note
      }
    } else {
      // In pause between notes or before first note
      uint32_t elapsed = currentTime - noteStartTime;
      uint16_t pauseTime = (currentNoteIndex == 0) ? 0 : currentMelody->pauseBetweenNotes;

      if (elapsed >= pauseTime) {
        // Start next note
        if (currentNoteIndex < currentMelody->noteCount) {
          setFrequency(currentMelody->notes[currentNoteIndex].frequency);
          setPwmOn();
          noteIsOn = true;
          noteStartTime = currentTime;
        } else {
          // Melody finished - repeat if needed
          if (melodyRepeat) {
            currentNoteIndex = 0;  // Restart melody
            noteStartTime = currentTime;
          } else {
            stop();
            return;
          }
        }
      }
    }
    return;
  }

  // Handle simple beep playback
  if (!playing) {
    if (isOn) {
      isOn = false;
      setPwmOff();
    }
    return;
  }

  uint32_t elapsed = currentTime - startTime;

  if (isOn) {
    if (elapsed >= beepDuration) {
      setPwmOff();
      isOn = false;
      startTime = currentTime;
      if (++currentRepetition >= repetitions) {
        stop();
        return;
      }
    }
    return;
  }

  if (elapsed >= pauseDuration) {
    setPwmOn();
    isOn = true;
    startTime = currentTime;
  }
}

void Buzzer::startBeep(uint16_t duration, uint8_t reps, uint16_t pause, uint16_t frequency) {
  if (playing || playingMelody) {
    stop();
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
}

void Buzzer::startMelody(const Melody* melody, bool repeat) {
  if (playing || playingMelody) {
    stop();
  }

  currentMelody = melody;
  currentNoteIndex = 0;
  playingMelody = true;
  noteIsOn = false;
  noteStartTime = millis();
  melodyRepeat = repeat;
}

void Buzzer::setFrequency(uint16_t frequency) {
  if (frequency == 0) {
    frequency = pwmFrequency; // Use default if 0
  }

  // Update frequency if it changed
  if (frequency != pwmFrequency) {
    pwmFrequency = frequency;
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, pwmFrequency);
  }
}

// Musical note frequencies (Hz) - more precise
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_G5  784

// Melody definitions - more musical and less "beep-like"
static const Note systemStartNotes[] = {
  {NOTE_C4, 120},  // C4 - 120ms
  {NOTE_E4, 120},  // E4 - 120ms
  {NOTE_G4, 120},  // G4 - 120ms
  {NOTE_C5, 200}   // C5 - 200ms (higher octave for fanfare effect)
};

static const Melody systemStartMelody = {
  systemStartNotes,
  4,
  30  // 30ms pause between notes (faster, more musical)
};

static const Note calibrationCompleteNotes[] = {
  {NOTE_G4, 100},  // G4 - 100ms
  {NOTE_E4, 100},  // E4 - 100ms
  {NOTE_C4, 100},  // C4 - 100ms
  {NOTE_G4, 150},  // G4 - 150ms (repeat for confirmation)
  {NOTE_C5, 250}   // C5 - 250ms (final note)
};

static const Melody calibrationCompleteMelody = {
  calibrationCompleteNotes,
  5,
  40  // 40ms pause between notes
};

static const Note disarmedNotes[] = {
  {NOTE_G4, 150},  // G4 - 150ms
  {NOTE_E4, 150},  // E4 - 150ms
  {NOTE_C4, 300}   // C4 - 300ms (longer final note)
};

static const Melody disarmedMelody = {
  disarmedNotes,
  3,
  60  // 60ms pause between notes
};

static const Note armingBlockedNotes[] = {
  {NOTE_E5, 80},   // E5 - 80ms (high note for alert)
  {NOTE_E5, 80},   // E5 - 80ms
  {NOTE_E5, 120}   // E5 - 120ms (slightly longer)
};

static const Melody armingBlockedMelody = {
  armingBlockedNotes,
  3,
  50  // 50ms pause between notes
};

static const Note armedAlertNotes[] = {
  {NOTE_F4, 300},  // F4 - 300ms
  {NOTE_A4, 300}   // A4 - 300ms (alternating for less monotony)
};

static const Melody armedAlertMelody = {
  armedAlertNotes,
  2,
  100  // 100ms pause between notes
};

// New contextual methods
void Buzzer::beepSystemStart() {
  startMelody(&systemStartMelody);
}

void Buzzer::beepCalibrationStep() {
  // Musical note instead of beep - ascending tone for progress
  startBeep(120, 1, 0, NOTE_D4);  // D4 note, 120ms
}

void Buzzer::beepCalibrationComplete() {
  startMelody(&calibrationCompleteMelody);
}

void Buzzer::beepDisarmed() {
  startMelody(&disarmedMelody);
}

void Buzzer::beepArmingBlocked() {
  // Musical sequence instead of repetitive beeps
  startMelody(&armingBlockedMelody);
}

void Buzzer::beepButtonClick() {
  // Short musical note - soft and pleasant
  startBeep(60, 1, 0, NOTE_A4);  // A4 note, 60ms - pleasant frequency
}

void Buzzer::beepArmedAlert() {
  // Continuous repeating melody - alternates between two notes for less monotony
  // Musical and alerting, but not harsh like a simple beep
  startMelody(&armedAlertMelody, true);  // true = repeat continuously
}

void Buzzer::setPwmOn() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel, pwmDutyCycle);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel);
}

void Buzzer::setPwmOff() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel);
}

void Buzzer::stop() {
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