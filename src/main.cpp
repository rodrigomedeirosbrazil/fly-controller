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
}

void loop()
{
  unsigned long now = millis();

  screen.draw();
  checkCanbus();

  if (RC_avail() || now - lastRcUpdate > PWM_FRAME_TIME_MS)
  {
    lastRcUpdate = now;

    pwmReader.tick(RC_decode(THROTTLE_CHANNEL) * 100);

    throttle.tick();

    handleEsc();
  }
}

void handleEsc()
{
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
    throttle.isCruising() 
      ? throttle.getCruisingThrottlePosition()
      : throttle.getThrottlePercentageFiltered(),
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