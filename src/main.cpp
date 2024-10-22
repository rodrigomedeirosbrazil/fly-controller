#include <Arduino.h>

#include <Servo.h>

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
Screen screen(&display, &throttle);
Canbus canbus;

unsigned long lastRcUpdate;

void setup()
{
  #if SERIAL_DEBUG
    Serial.begin(9600);
  #endif

  display.begin();
  display.setFlipMode(1);

  setup_pwmRead();
  esc.attach(ESC_PIN, ESC_MIN, ESC_MAX);

  // just to power the receiver
  // pinMode(2, OUTPUT);
  // digitalWrite(2, HIGH);
}

void loop()
{
  unsigned long now = millis();

  screen.draw();
  canbus.tick();

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
  int pulseWidth = ESC_MIN;

  if (!throttle.isArmed())
  {
    esc.writeMicroseconds(pulseWidth);
    return;
  }

  pulseWidth = map(
    throttle.isCruising() 
      ? throttle.getCruisingThrottlePosition()
      : throttle.getThrottlePercentageFiltered(),
    0, 
    100, 
    ESC_MIN, 
    ESC_MAX
  );

    esc.writeMicroseconds(
      throttle.isArmed()
      ? pulseWidth
      : 0
    );

    return;
}