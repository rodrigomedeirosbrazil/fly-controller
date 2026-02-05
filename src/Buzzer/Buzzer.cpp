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
  noteIsOn(false) {
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
          // Melody finished
          stop();
          return;
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

void Buzzer::startMelody(const Melody* melody) {
  if (playing || playingMelody) {
    stop();
  }

  currentMelody = melody;
  currentNoteIndex = 0;
  playingMelody = true;
  noteIsOn = false;
  noteStartTime = millis();
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

// Melody definitions
static const Note systemStartNotes[] = {
  {262, 150},  // C4 (Do) - 150ms
  {330, 150},  // E4 (Mi) - 150ms
  {392, 200}   // G4 (Sol) - 200ms
};

static const Melody systemStartMelody = {
  systemStartNotes,
  3,
  50  // 50ms pause between notes
};

static const Note calibrationCompleteNotes[] = {
  {392, 150},  // G4 (Sol) - 150ms
  {330, 150},  // E4 (Mi) - 150ms
  {262, 200}   // C4 (Do) - 200ms
};

static const Melody calibrationCompleteMelody = {
  calibrationCompleteNotes,
  3,
  50  // 50ms pause between notes
};

static const Note disarmedNotes[] = {
  {392, 200},  // G4 (Sol) - 200ms
  {262, 300}   // C4 (Do) - 300ms (longer for finalization)
};

static const Melody disarmedMelody = {
  disarmedNotes,
  2,
  100  // 100ms pause between notes
};

// New contextual methods
void Buzzer::beepSystemStart() {
  startMelody(&systemStartMelody);
}

void Buzzer::beepCalibrationStep() {
  // Single beep at medium frequency for each calibration step
  startBeep(150, 1, 0, 2500);  // 150ms, medium frequency
}

void Buzzer::beepCalibrationComplete() {
  startMelody(&calibrationCompleteMelody);
}

void Buzzer::beepDisarmed() {
  startMelody(&disarmedMelody);
}

void Buzzer::beepArmingBlocked() {
  // 3 quick beeps at higher frequency (alert)
  startBeep(100, 3, 80, 3500);  // 100ms beeps, 80ms pause, high frequency
}

void Buzzer::beepButtonClick() {
  // Very short, soft beep for button feedback
  startBeep(50, 1, 0, 2000);  // 50ms, lower frequency for softer sound
}

void Buzzer::beepArmedAlert() {
  // Continuous intermittent beep - critical safety alert
  // 500ms beep, 200ms pause, repeating (255 = continuous)
  startBeep(500, 255, 200, 3000);  // Medium-high frequency for alert
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
  setPwmOff();
}