#include <stdint.h>

#include "Buzzer.h"

namespace {
// Empirical tuning notes for the current hardware:
// - Supply: 3.3 V
// - Transistor stage: BC337
// - Sounder: passive piezo buzzer
// Device testing showed the strongest output near 85% duty cycle. A sweep also
// showed that 2000-2500 Hz stays in the loudest range, while higher
// frequencies lose volume. We use 2300 Hz for general UX beeps and 2000 Hz for
// the armed alert so it remains distinct while staying in the loud range.
constexpr uint16_t kDefaultBeepFrequencyHz = 2300;
constexpr uint16_t kAlertBeepFrequencyHz = 2000;
constexpr uint8_t kDefaultDutyCycle = 217;  // 85%
}

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),       // Use channel 1 to avoid conflict with ESP32Servo (uses timer 0)
  pwmFrequency(kDefaultBeepFrequencyHz),
  pwmResolution(8),    // 8-bit resolution (0-255)
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
  melodyRepeat(false) {
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

// General UX beeps use the tuned default frequency and duty cycle.
// The armed alert uses a slightly lower tone so it stands out immediately.

void Buzzer::beepSystemStart() {
  // 3 beeps — friendly startup confirmation
  startBeep(150, 3, 80, kDefaultBeepFrequencyHz);
}

void Buzzer::beepCalibrationStep() {
  // 2 short beeps — step acknowledgement
  startBeep(80, 2, 60, kDefaultBeepFrequencyHz);
}

void Buzzer::beepCalibrationComplete() {
  // 3 longer beeps — positive confirmation
  startBeep(200, 3, 80, kDefaultBeepFrequencyHz);
}

void Buzzer::beepDisarmed() {
  // 2 slow beeps — relaxed, disarmed state
  startBeep(250, 2, 150, kDefaultBeepFrequencyHz);
}

void Buzzer::beepArmingBlocked() {
  // 5 rapid beeps — urgent warning
  startBeep(60, 5, 40, kDefaultBeepFrequencyHz);
}

void Buzzer::beepButtonClick() {
  // 1 short beep — tactile click feedback
  startBeep(50, 1, 0, kDefaultBeepFrequencyHz);
}

void Buzzer::beepArmedAlert() {
  // Fast continuous beep — repeating armed alert
  startBeep(200, 255, 200, kAlertBeepFrequencyHz);
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
