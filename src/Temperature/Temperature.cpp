#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

double Temperature::readTemperature() {
  int sum = 0;
  for (int i = 0; i < SAMPLES_FOR_FILTER; i++) {
    sum += analogRead(MOTOR_TEMPERATURE_PIN);
  }
  
  double v = (vcc * sum) / (SAMPLES_FOR_FILTER * 1024.0);
  double rt = (vcc * R) / v - R;
  double t = beta / log(rt / rx);
  return t - 273.0;
}