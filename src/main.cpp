#include <Arduino.h>

#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>

#include "config.h"
#include "main.h"
#include "PWMread_RCfailsafe.h"

#include "Throttle/Throttle.h"
#include "PwmReader/PwmReader.h"
#include "Display/Display.h"
#include "Screen/Screen.h"
#include "Canbus/Canbus.h"

Servo esc;
PwmReader pwmReader;
Throttle throttle(&pwmReader);
Display display;
Canbus canbus;
Screen screen(&display, &throttle, &canbus);

MCP2515 mcp2515(CANBUS_CS_PIN);
struct can_frame canMsg;

unsigned long lastRcUpdate;

unsigned long currentLimitReachedTime;
bool isCurrentLimitReached;

void setup()
{
  #if SERIAL_DEBUG
    Serial.begin(9600);
  #endif

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  display.begin();
  display.setFlipMode(1);

  setup_pwmRead();

  currentLimitReachedTime = 0;
  isCurrentLimitReached = false;
}

void loop()
{
  unsigned long now = millis();

  screen.draw();
  checkCanbus();

  if (RC_avail() || now - lastRcUpdate > PWM_FRAME_TIME_MS) {
    lastRcUpdate = now;

    pwmReader.tick(RC_decode(THROTTLE_CHANNEL) * 100);

    throttle.tick();

    handleEsc();
  }
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
      throttle.getThrottlePercentageFiltered()
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
