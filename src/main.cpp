#include <Arduino.h>

#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>
#include <AceButton.h>

#include "config.h"
#include "main.h"

#include "Throttle/Throttle.h"
#include "SerialScreen/SerialScreen.h"

#include "Canbus/Canbus.h"
#include "Temperature/Temperature.h"
#include "Buzzer/Buzzer.h"
#include "Power/Power.h"

using namespace ace_button;
#include "Button/Button.h"

void setup()
{
  screen.init();
  buzzer.setup();
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  pinMode(PIN_WIRE_SDA, OUTPUT);
  pinMode(PIN_WIRE_SCL, OUTPUT);
  digitalWrite(PIN_WIRE_SDA, LOW);
  digitalWrite(PIN_WIRE_SCL, LOW);

  canbus.setLedColor(Canbus::ledColorRed);
  buzzer.beepWarning();
}

void loop()
{
  button.check();
  screen.write();
  checkCanbus();
  throttle.handle();
  motorTemp.handle();
  buzzer.handle();

  handleEsc();
}

void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
{
  button.handleEvent(aceButton, eventType, buttonState);
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

  int pulseWidth = power.getPwm();
  esc.writeMicroseconds(pulseWidth);
}

void checkCanbus()
{
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        canbus.parseCanMsg(&canMsg);
    }
}
