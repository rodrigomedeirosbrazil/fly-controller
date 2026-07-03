#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

Temperature::Temperature(ReadFn readFn, float adcVoltageRef) {
  this->readFn = readFn;
  this->adcVoltageRef = adcVoltageRef;

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
  memmove(
    &pinValues,
    &pinValues[1],
    sizeof(pinValues[0]) * (samples - 1)
  );

  int oversampledValue = readFn();
  pinValues[samples - 1] = oversampledValue;

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  // Voltage at the divider point: ReadFn returns 0-4095, scale by adcVoltageRef
  float v = (adcVoltageRef * (float)sum) / (samples * 4095.0f);

  // Solving for rt: rt = (v * R) / (VCC - v)
  // VCC is 3.3V (sensor power supply)
  const float vcc = 3.3f;
  float rt = (v * R) / (vcc - v);

  // Steinhart-Hart equation using Beta coefficient:
  // 1/T = 1/T0 + (1/B) * ln(Rt/R0)
  // T = 1 / (1/T0 + (1/B) * ln(Rt/R0))
  float invT = (1.0f / t0) + (1.0f / beta) * logf(rt / r0);
  float tempK = 1.0f / invT;
  temperature = tempK - 273.15f;
}
