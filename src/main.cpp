#include <Arduino.h>

#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>
#include <AceButton.h>

#include "config.h"
#include "main.h"

#include "Throttle/Throttle.h"

#if ENABLED_DISPLAY
  #include "Display/Display.h"
  #include "Screen/Screen.h"
#endif

#include "Canbus/Canbus.h"
#include "Temperature/Temperature.h"

using namespace ace_button;
#include "Button/Button.h"

MCP2515 mcp2515(CANBUS_CS_PIN);

Servo esc;
Throttle throttle;
Canbus canbus(&mcp2515);
Button button(BUTTON_PIN, &throttle);
Temperature motorTemp(MOTOR_TEMPERATURE_PIN);
AceButton aceButton(BUTTON_PIN);

#if ENABLED_DISPLAY
  Display display;
  Screen screen(&display, &throttle, &canbus, &motorTemp);
#endif

struct can_frame canMsg;

unsigned long lastSerialUpdate;

unsigned long currentLimitReachedTime;
bool isCurrentLimitReached;

void setup()
{
  Serial.begin(9600);
  Serial.println("armed,throttlePercentage,motorTemp,voltage,current,temp,rpm");

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  #if ENABLED_DISPLAY
    display.setBusClock(200000);
    display.begin();
    display.setFlipMode(0);
  #endif

  #if ENABLED_DISPLAY == false
    pinMode(PIN_WIRE_SDA, OUTPUT);
    pinMode(PIN_WIRE_SCL, OUTPUT);
    digitalWrite(PIN_WIRE_SDA, LOW);
    digitalWrite(PIN_WIRE_SCL, LOW);
  #endif

  currentLimitReachedTime = 0;
  isCurrentLimitReached = false;

  canbus.setLedColor(Canbus::ledColorRed);
}

void loop()
{
  button.check();

  #if ENABLED_DISPLAY
    screen.draw();
  #endif
  
  checkCanbus();
  handleSerialLog();
  throttle.handle();
  motorTemp.handle();
  handleEsc();
}

void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
{
  button.handleEvent(aceButton, eventType, buttonState);
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
      canbus.setLedColor(Canbus::ledColorRed);
    }
    return;
  }

  if (!throttle.isArmed())
  {
    if (esc.attached()) {
      esc.detach();
      canbus.setLedColor(Canbus::ledColorRed);
    }
    return;
  }

  if (!esc.attached())
  {
    esc.attach(ESC_PIN);
    canbus.setLedColor(Canbus::ledColorGreen);
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
