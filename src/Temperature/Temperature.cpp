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

  // ESP32-C3: 12-bit ADC (0-4095) with 3.3V reference
  double v = (ADC_VREF * sum) / (samples * ADC_MAX_VALUE);
  double rt = (ADC_VREF * R) / v - R;
  double t = beta / log(rt / rx);
  temperature = t - 273.0;
}
