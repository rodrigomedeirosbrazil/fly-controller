#include "Buzzer.h"

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),       // Use channel 1 to avoid conflict with ESP32Servo (uses timer 0)
  pwmFrequency(2500),  // 2.5 kHz - typical frequency for passive buzzers
  pwmResolution(8),    // 8-bit resolution (0-255)
  pwmDutyCycle(128),   // 50% duty cycle for medium volume
  playing(false),
  startTime(0),
  beepDuration(0),
  pauseDuration(0),
  repetitions(0),
  currentRepetition(0),
  isOn(false) {
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
  if (!playing) {
    if (isOn) {
      isOn = false;
      setPwmOff();
    }
    return;
  }

  uint32_t currentTime = millis();
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

void Buzzer::startBeep(uint16_t duration, uint8_t reps, uint16_t pause) {
  if (playing) {
    stop();
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

void Buzzer::beepSuccess() {
  startBeep(200, 2, 200);
}

void Buzzer::beepError() {
  startBeep(1000, 1, 0);
}

void Buzzer::beepWarning() {
  startBeep(200, 4, 200);
}

void Buzzer::beepCustom(uint16_t duration, uint8_t repetitions) {
  startBeep(duration, repetitions, 100);
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
  isOn = false;
  repetitions = 0;
  setPwmOff();
}