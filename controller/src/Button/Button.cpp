#include <Arduino.h>
#include <AceButton.h>

#include "../config.h"
#include "Button.h"
#include "../Throttle/Throttle.h"
#include "../Buzzer/Buzzer.h"
#include "../RemoteLink/RemoteLink.h"
#include "../Settings/Settings.h"
#include "../main.h"

extern Throttle throttle;
extern Buzzer buzzer;
extern Settings settings;

int SourceSwitchButtonConfig::readButton(uint8_t pin) {
    if (settings.getThrottleSource() == ThrottleSourceWireless) {
        // Active-low semantics to match INPUT_PULLUP: pressed -> LOW.
        return remoteLink.remoteButtonPressed() ? LOW : HIGH;
    }
    return digitalRead(pin);
}

Button::Button(
  uint8_t pin
) {
    this->pin = pin;
    pinMode(pin, INPUT_PULLUP);
    aceButton.setButtonConfig(&sourceConfig);
    aceButton.init(pin);
    releaseButtonTime = 0;
    buttonWasClicked = false;

    buttonConfig = aceButton.getButtonConfig();

    buttonConfig->setEventHandler(handleButtonEvent);
    buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
    buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
    buttonConfig->setLongPressDelay(2000);
    buttonConfig->setClickDelay(300);
}

void Button::check()
{
    aceButton.check();
}

void Button::handleEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
{
  switch (eventType) {
    case AceButton::kEventClicked:
      buttonWasClicked = true;
      break;
    case AceButton::kEventReleased:
      if (buttonWasClicked) {
        buzzer.beepButtonClick();

        releaseButtonTime = millis();
        buttonWasClicked = false;
      }
      break;
    case AceButton::kEventLongPressed:
      if (
        !buttonWasClicked
        && (millis() - releaseButtonTime <= longClickThreshold)
        && !throttle.isArmed()
      ) {
        throttle.setArmed();
        break;
      }

      if (throttle.isArmed()) {
        throttle.setDisarmed();
      }
      break;
  }
}
