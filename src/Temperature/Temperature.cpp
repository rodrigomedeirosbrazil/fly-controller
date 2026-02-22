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
  memcpy(
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
  double v = (adcVoltageRef * (double)sum) / (samples * 4095.0);

  // Solving for rt: rt = (v * R) / (VCC - v)
  // VCC is 3.3V (sensor power supply)
  const double vcc = 3.3;
  double rt = (v * R) / (vcc - v);

  // Steinhart-Hart equation using Beta coefficient:
  // 1/T = 1/T0 + (1/B) * ln(Rt/R0)
  // T = 1 / (1/T0 + (1/B) * ln(Rt/R0))
  double invT = (1.0 / t0) + (1.0 / beta) * log(rt / r0);
  double tempK = 1.0 / invT;
  temperature = tempK - 273.15;
}
