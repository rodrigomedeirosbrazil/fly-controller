#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

Temperature::Temperature(uint8_t pin) {
  this->pin = pin;

  memset(
    &pinValues, 
    0,
    sizeof(pinValues[0]) * samples
  );

  temperature = 0;
  lastPinRead = 0;
}

void Temperature::handle()
{
  unsigned long now = millis();

  if (now - lastPinRead < 100) {
    return;
  }

  lastPinRead = now;
  readTemperature();
}

void Temperature::readTemperature() {
  memcpy(
    &pinValues, 
    &pinValues[1], 
    sizeof(pinValues[0]) * (samples - 1)
  );

  pinValues[samples - 1] = analogRead(pin);

  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }
  
  double v = (vcc * sum) / (samples * 1024.0);
  double rt = (vcc * R) / v - R;
  double t = beta / log(rt / rx);
  temperature = t - 273.0;
}
