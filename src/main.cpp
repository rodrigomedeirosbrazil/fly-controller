#include <Arduino.h>

#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>
#include <AceButton.h>

#include "config.h"
#include "main.h"

#include "Throttle/Throttle.h"
#include "Display/Display.h"
#include "Screen/Screen.h"
#include "Canbus/Canbus.h"
#include "Temperature/Temperature.h"

using namespace ace_button;

Servo esc;
Throttle throttle;
Display display;
Canbus canbus;
Temperature motorTemp;
Screen screen(&display, &throttle, &canbus, &motorTemp);
AceButton aceButton(BUTTON_PIN);

MCP2515 mcp2515(CANBUS_CS_PIN);
struct can_frame canMsg;

unsigned long lastSerialUpdate;

unsigned long currentLimitReachedTime;
bool isCurrentLimitReached;

unsigned long releaseButtonTime = 0;
bool buttonWasClicked = false;
const unsigned long longClickThreshold = 3500;

void setup()
{
  Serial.begin(9600);
  Serial.println("armed,throttlePercentage,motorTemp,voltage,current,temp,rpm");

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  display.setBusClock(300000);
  display.begin();
  display.setFlipMode(0);

  currentLimitReachedTime = 0;
  isCurrentLimitReached = false;

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  ButtonConfig* buttonConfig = aceButton.getButtonConfig();
  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureRepeatPress);
}

void loop()
{
  screen.draw();
  checkCanbus();
  handleSerialLog();
  throttle.handle();
  motorTemp.handle();
  handleEsc();
  aceButton.check();
}

void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
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
      if (!buttonWasClicked && (millis() - releaseButtonTime <= longClickThreshold)) {
        throttle.setArmed();
      } else {
        throttle.setDisarmed();
      }
      break;
  }
}

void handleSerialLog() {
  if (millis() - lastSerialUpdate < 1000) {
    return;
  }

  lastSerialUpdate = millis();

  Serial.print(throttle.isArmed());
  Serial.print(",");

  Serial.print(throttle.getThrottlePercentage());
  Serial.print(",");

  Serial.print(motorTemp.getTemperature());
  Serial.print(",");

  if (canbus.isReady()) {
    Serial.print(canbus.getMiliVoltage());
    Serial.print(",");

    Serial.print(canbus.getMiliCurrent());
    Serial.print(",");

    Serial.print(canbus.getTemperature());
    Serial.print(",");

    Serial.print(canbus.getRpm());
    Serial.print(",");
  }

  if (!canbus.isReady()) {
    Serial.print(",,,,");
  }

  Serial.println();
}

void handleEsc()
{
  if (canbus.isReady() && canbus.getMiliVoltage() <= BATTERY_MIN_VOLTAGE)
  {
    if (esc.attached()) {
      esc.detach();
    }
    return;
  }

  if (!throttle.isArmed())
  {
    if (esc.attached()) {
      esc.detach();
    }
    return;
  }

  if (!esc.attached())
  {
    esc.attach(ESC_PIN);
  }

  int pulseWidth = ESC_MIN_PWM;

  pulseWidth = map(
    analizeTelemetryToThrottleOutput(
      throttle.getThrottlePercentage()
    ),
    0, 
    100, 
    ESC_MIN_PWM, 
    ESC_MAX_PWM
  );

  esc.writeMicroseconds(pulseWidth);

  return;
}

void checkCanbus() 
{
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        canbus.parseCanMsg(&canMsg);
    }
}

unsigned int analizeTelemetryToThrottleOutput(unsigned int throttlePercentage)
{
  #if ENABLED_LIMIT_THROTTLE
  if (
    canbus.isReady() 
    && canbus.getTemperature() >= ESC_MAX_TEMP)
  {
    throttle.cancelCruise();

    return throttlePercentage < THROTTLE_RECOVERY_PERCENTAGE
      ? throttlePercentage
      : THROTTLE_RECOVERY_PERCENTAGE;
  }

  if (motorTemp.getTemperature() >= MOTOR_MAX_TEMP) {
    throttle.cancelCruise();

    return throttlePercentage < THROTTLE_RECOVERY_PERCENTAGE
      ? throttlePercentage
      : THROTTLE_RECOVERY_PERCENTAGE;
  }

  unsigned int miliCurrentLimit = ESC_MAX_CURRENT < BATTERY_MAX_CURRENT
    ? ESC_MAX_CURRENT * 10
    : BATTERY_MAX_CURRENT * 10;

  if (
    canbus.isReady() 
    && canbus.getMiliCurrent() >= miliCurrentLimit
  ) {
    throttle.cancelCruise();

    if (!isCurrentLimitReached) {
      isCurrentLimitReached = true;
      currentLimitReachedTime = millis();
      return throttlePercentage;
    }

    if (millis() - currentLimitReachedTime > 10000) {
      return throttlePercentage < THROTTLE_RECOVERY_PERCENTAGE
        ? throttlePercentage
        : THROTTLE_RECOVERY_PERCENTAGE;
    }

    if (millis() - currentLimitReachedTime > 3000) {
      return throttlePercentage - 10;
    }

    return throttlePercentage;
  }

  isCurrentLimitReached = false;
  currentLimitReachedTime = 0;

  #endif

  return throttle.isCruising() 
      ? throttle.getCruisingThrottlePosition()
      : throttlePercentage;
}
