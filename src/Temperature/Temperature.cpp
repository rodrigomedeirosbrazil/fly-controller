#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

Temperature::Temperature() {
  memset(
    &pinValues, 
    0,
    sizeof(pinValues[0]) * SAMPLES_FOR_FILTER
  );

  temperature = 0;
  lastPinRead = 0;
}

void Temperature::tick()
{
  unsigned long now = millis();

  if (now - lastPinRead < 10) {
    return;
  }

  lastPinRead = now;
  readTemperature();
}

void Temperature::readTemperature() {
  memcpy(
    &pinValues[1], 
    &pinValues, 
    sizeof(pinValues[0]) * (SAMPLES_FOR_FILTER - 1)
  );

  pinValues[SAMPLES_FOR_FILTER - 1] = analogRead(MOTOR_TEMPERATURE_PIN);

  int sum = 0;
  for (int i = 0; i < SAMPLES_FOR_FILTER; i++) {
    sum += pinValues[i];
  }
  
  double v = (vcc * sum) / (SAMPLES_FOR_FILTER * 1024.0);
  double rt = (vcc * R) / v - R;
  double t = beta / log(rt / rx);
  temperature = t - 273.0;
}