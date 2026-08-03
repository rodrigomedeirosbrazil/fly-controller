#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include <driver/ledc.h>

// Passive-piezo tone driver via ESP32 LEDC PWM. Pure hardware concern: knows
// how to turn a tone on/off at a given frequency and how loud. All timing,
// pattern, and priority policy lives in Sound/ (see Sound.h / SoundLogic.h).
class Buzzer {
public:
  Buzzer(uint8_t buzzerPin);
  void setup();

  // Re-applies the timer frequency after the global LEDC clock source
  // changes (e.g. when ESP32Servo's esc.attach() forces XTAL on the
  // shared low-speed clock, halving previously-configured frequencies).
  void recalibrate();

  // Sets the output volume as a percentage (0-100), mapped directly to the
  // PWM duty cycle. 0 = silent. Takes effect on the next toneOn().
  void setVolume(uint8_t percent);

  // Starts/retunes the tone at the given frequency. Safe to call while
  // already on -- silently switches frequency without an intermediate
  // toneOff() (callers that need to avoid the audible click of an in-place
  // frequency change should call toneOff() first; see Sound::handle()).
  void toneOn(uint16_t frequencyHz);
  void toneOff();

private:
  uint8_t pin;
  uint8_t pwmChannel;
  uint32_t pwmFrequency;
  uint8_t pwmResolution;
  uint32_t pwmDutyCycle;

  void setPwmOn();
  void setPwmOff();
};

#endif
