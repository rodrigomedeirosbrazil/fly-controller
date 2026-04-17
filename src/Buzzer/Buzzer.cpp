#include "Buzzer.h"

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),       // Use channel 1 to avoid conflict with ESP32Servo (uses timer 0)
  pwmFrequency(2500),  // 2.5 kHz - passive buzzer resonant frequency for maximum volume
  pwmResolution(8),    // 8-bit resolution (0-255)
  pwmDutyCycle(255),   // 100% duty cycle — narrow low-pulse generates harmonics that excite buzzer resonance
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

// All beep patterns use 2500 Hz (buzzer resonant frequency) at 100% duty cycle.
// With 255/256 duty, the narrow low-pulse generates harmonics that excite the buzzer
// at its resonant frequency regardless of the fundamental, maximizing volume.
// Sounds are distinguished by rhythm (count, duration, pause) rather than pitch.

void Buzzer::beepSystemStart() {
  // 3 beeps — friendly startup confirmation
  startBeep(150, 3, 80);
}

void Buzzer::beepCalibrationStep() {
  // 2 short beeps — step acknowledgement
  startBeep(80, 2, 60);
}

void Buzzer::beepCalibrationComplete() {
  // 3 longer beeps — positive confirmation
  startBeep(200, 3, 80);
}

void Buzzer::beepDisarmed() {
  // 2 slow beeps — relaxed, disarmed state
  startBeep(250, 2, 150);
}

void Buzzer::beepArmingBlocked() {
  // 5 rapid beeps — urgent warning
  startBeep(60, 5, 40);
}

void Buzzer::beepButtonClick() {
  // 1 short beep — tactile click feedback
  startBeep(50, 1, 0);
}

void Buzzer::beepArmedAlert() {
  // Fast continuous beep — repeating armed alert
  startBeep(200, 255, 200);
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
