#include <Arduino.h>
#include "SoftPWM.h"

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

unsigned long ledTimer = 0;
bool ledState = false;

void setup()
{
#if SERIAL_DEBUG
  Serial.begin(9600);
#endif
  display.begin();

  setup_pwmRead();

  SoftPWMBegin();
  SoftPWMSet(LED_BUILTIN, 0);
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

    setLed(now);

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
  if (millis() - lastDisplayUpdate < 1000)
  {
    return;
  }

  lastDisplayUpdate = millis();

  display.firstPage();
    do {
      screen.draw();
    } while (display.nextPage());
}

void setLed(unsigned long now)
{
  if (!throttle.isArmed())
  {
    if (now - ledTimer > 250)
    {
      ledTimer = now;
      ledState = !ledState;
      SoftPWMSetPercent(LED_BUILTIN, ledState == true ? 100 : 0);
    }
  }

  if (throttle.isArmed() && throttle.isCruising())
  {
    if (now - ledTimer > 100)
    {
      ledTimer = now;
      ledState = !ledState;
      SoftPWMSetPercent(LED_BUILTIN, ledState == true ? 100 : 0);
    }
  }

  if (throttle.isArmed() && !throttle.isCruising())
  {
    SoftPWMSetPercent(
      LED_BUILTIN, 
      throttle.getThrottlePercentageFiltered()
    );
  }
}