#include <Arduino.h>

#include <Servo.h>

#include "config.h"
#include "main.h"
#include "PWMread_RCfailsafe.h"

#include "Throttle/Throttle.h"
#include "PwmReader/PwmReader.h"
#include "Display/Display.h"
#include "Screen/Screen.h"

Servo esc;
PwmReader pwmReader;
Throttle throttle(&pwmReader);
Display display;
Screen screen(&display, &throttle);

unsigned long lastRcUpdate;

void setup()
{
  #if SERIAL_DEBUG
    Serial.begin(9600);
  #endif

  display.begin();

  setup_pwmRead();
  esc.attach(ESC_PIN, ESC_MIN, ESC_MAX);

  // just to power the receiver
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
}

void loop()
{
  unsigned long now = millis();
  screen.draw();

  if (RC_avail() || now - lastRcUpdate > PWM_FRAME_TIME_MS)
  {
    lastRcUpdate = now;

    pwmReader.tick(RC_decode(THROTTLE_CHANNEL) * 100);

    throttle.tick();

    handleEsc();

    #if SERIAL_DEBUG
      Serial.print(throttle.isArmed());
      Serial.print(" ");
      Serial.print(throttle.isCruising());
      Serial.print(" ");
      Serial.print(pwmReader.getThrottlePercentage());
      Serial.print(" ");
      Serial.print(throttle.getThrottlePercentageFiltered(pwmReader.getThrottlePercentage()));
      Serial.print(" ");
      Serial.print(throttle.getCruisingThrottlePosition());
      Serial.println();
    #endif
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

    esc.writeMicroseconds(pulseWidth);
    return;
}