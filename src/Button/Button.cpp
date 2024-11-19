#include <Arduino.h>
#include <AceButton.h>

#include "../config.h"
#include "Button.h"
#include "../Throttle/Throttle.h"
#include "../main.h"

Button::Button(uint8_t pin, Throttle *throttle)
{
    this->pin = pin;
    this->throttle = throttle;
    pinMode(pin, INPUT_PULLUP);
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
        releaseButtonTime = millis();
        buttonWasClicked = false;
      }
      break;
    case AceButton::kEventLongPressed:
      if (
        !buttonWasClicked 
        && (millis() - releaseButtonTime <= longClickThreshold)
        && !throttle->isArmed()
      ) {
        throttle->setArmed();
        break;
      }
      
      if (
        !buttonWasClicked 
        && throttle->isArmed()
      ) {
        throttle->setDisarmed();
      }
      break;
  }
}
