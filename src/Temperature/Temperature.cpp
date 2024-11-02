#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

double Temperature::readTemperature() {
  int sum = 0;
  for (int i = 0; i < nSamples; i++) {
    sum += analogRead(MOTOR_TEMPERATURE_PIN);
    delay (10);
  }
  
  double v = (vcc*sum)/(nSamples*1024.0);
  double rt = (vcc*R)/v - R;
  double t = beta / log(rt/rx);
  return t-273.0;
}