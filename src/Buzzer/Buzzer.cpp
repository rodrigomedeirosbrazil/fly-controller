#include <stdint.h>

#include "Buzzer.h"

namespace {
// Empirical tuning notes for the current hardware:
// - Supply: 3.3 V
// - Transistor stage: BC337
// - Sounder: passive piezo buzzer
// Device testing showed the strongest output near 85% duty cycle. A sweep also
// showed that 2000-2500 Hz stays in the loudest range, while higher
// frequencies lose volume.
constexpr uint16_t kDefaultFrequencyHz = 2300;
constexpr uint8_t kDefaultDutyCycle = 217;  // 85%
}

Buzzer::Buzzer(uint8_t buzzerPin) :
  pin(buzzerPin),
  pwmChannel(1),       // Use channel 1 to avoid conflict with ESP32Servo (uses timer 0)
  pwmFrequency(kDefaultFrequencyHz),
  pwmResolution(8),    // 8-bit resolution (0-255)
  pwmDutyCycle(kDefaultDutyCycle) {
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

void Buzzer::setVolume(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  // Map 0-100% directly to the 8-bit duty cycle (0-255). 0% = silent.
  pwmDutyCycle = (uint32_t)percent * 255 / 100;
}

void Buzzer::toneOn(uint16_t frequencyHz) {
  if (frequencyHz == 0) {
    frequencyHz = pwmFrequency;
  }
  if (frequencyHz != pwmFrequency) {
    pwmFrequency = frequencyHz;
    ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, pwmFrequency);
  }
  setPwmOn();
}

void Buzzer::toneOff() {
  setPwmOff();
}

void Buzzer::setPwmOn() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel, pwmDutyCycle);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel);
}

void Buzzer::setPwmOff() {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)pwmChannel);
}
