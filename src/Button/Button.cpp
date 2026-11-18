#include <Arduino.h>

#include "../config.h"
#include "Button.h"
#include "../Throttle/Throttle.h"
#include "../Sound/Sound.h"

extern Throttle throttle;
extern Sound sound;

Button::Button(
  uint8_t pin
) : pin(pin), logic_(), powerScale_(100), armCharge_(0) {
    pinMode(pin, INPUT_PULLUP);
}

void Button::check()
{
    uint32_t now = millis();
    ButtonGestureOutput out = logic_.update(now, readRawPressed(), throttle.isArmed(), throttle.isEngaged());
    powerScale_ = out.powerScale;
    armCharge_ = out.armCharge;

    switch (out.intent) {
        case ButtonIntent::Click:
            sound.play(SoundEvent::ButtonClick);
            break;
        case ButtonIntent::Arm:
            throttle.setArmed();
            break;
        case ButtonIntent::Disarm:
            throttle.setDisarmed(DisarmReason::Manual);
            break;
        case ButtonIntent::None:
            break;
    }
}

bool Button::readRawPressed()
{
    // Active-low to match INPUT_PULLUP: pressed -> LOW.
    return digitalRead(pin) == LOW;
}
