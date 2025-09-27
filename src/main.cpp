#include <Arduino.h>

#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>
#include <AceButton.h>

#include "config.h"
#include "main.h"

#include "Throttle/Throttle.h"

#include "Canbus/Canbus.h"
#include "Hobbywing/Hobbywing.h"
#include "Temperature/Temperature.h"
#include "Buzzer/Buzzer.h"
#include "Power/Power.h"
#include "Xctod/Xctod.h"

using namespace ace_button;
#include "Button/Button.h"

void setup()
{
  xctod.init();
  buzzer.setup();
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  hobbywing.announce();
  hobbywing.requestEscId();
  hobbywing.setThrottleSource(Hobbywing::throttleSourcePWM);
  hobbywing.setLedColor(Hobbywing::ledColorGreen);

  buzzer.beepWarning();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_MIN_PWM);
}

void loop()
{
  button.check();
  xctod.write();
  checkCanbus();
  throttle.handle();
  motorTemp.handle();
  buzzer.handle();

  handleEsc();
  handleArmedBeep();
}

void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
{
  button.handleEvent(aceButton, eventType, buttonState);
}

void handleEsc()
{
  if (!throttle.isArmed())
  {
    esc.writeMicroseconds(ESC_MIN_PWM);
    return;
  }

  int pulseWidth = power.getPwm();
  esc.writeMicroseconds(pulseWidth);
}

void checkCanbus()
{
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        canbus.parseCanMsg(&canMsg);
    }
    hobbywing.announce();
}

bool isMotorRunning()
{
    // Check if ESC is ready and has RPM data
    if (hobbywing.isReady()) {
        uint16_t rpm = hobbywing.getRpm();
        // Consider motor running if RPM > 100 (adjust threshold as needed)
        return rpm > 10;
    }

    // Fallback: check throttle position
    // Consider motor running if throttle > 5%
    return throttle.getThrottlePercentage() > 5
     && throttle.isArmed();
}

void handleArmedBeep()
{
    static bool wasArmed = false;
    static bool wasMotorRunning = false;

    bool isArmed = throttle.isArmed();
    bool motorRunning = isMotorRunning();

    // Start continuous beep when armed and motor stops
    if (isArmed && !motorRunning && (!wasArmed || wasMotorRunning)) {
        buzzer.beepCustom(500, 255); // 500ms beep, 255 repetitions (continuous)
    }

    // Stop beep when motor starts running or throttle is disarmed
    if ((!isArmed || motorRunning) && wasArmed && !wasMotorRunning) {
        buzzer.stop();
    }

    wasArmed = isArmed;
    wasMotorRunning = motorRunning;
}
