#include <Arduino.h>

#include "config.h"
#include "main.h"
#include "PWMread_RCfailsafe.h"

#include "Throttle/Throttle.h"
#include "PwmReader/PwmReader.h"
#include "Display/Display.h"
#include "Screen/Screen.h"

PwmReader pwmReader;
Throttle throttle(&pwmReader);
Display display;
Screen screen(&display, &throttle);

unsigned long lastRcUpdate;
unsigned long lastDisplayUpdate;

void setup()
{
#if SERIAL_DEBUG
  Serial.begin(9600);
#endif
  display.begin();

  setup_pwmRead();

  // just to power the receiver
  pinMode(3, OUTPUT);
  digitalWrite(3, HIGH);
}

void loop()
{
  unsigned long now = millis();
  drawScreen();

  if (RC_avail() || now - lastRcUpdate > PWM_FRAME_TIME_MS)
  {
    lastRcUpdate = now;

    pwmReader.tick(RC_decode(THROTTLE_CHANNEL) * 100);

    throttle.tick();

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

void drawScreen()
{
  if (millis() - lastDisplayUpdate < 250)
  {
    return;
  }

  lastDisplayUpdate = millis();

  display.firstPage();
    do {
      screen.draw();
    } while (display.nextPage());
}
