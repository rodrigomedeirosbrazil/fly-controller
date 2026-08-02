#include <Arduino.h>

#include "../config.h"
#include "Temperature.h"

Temperature::Temperature(ReadFn readFn, ReadOkFn readOkFn, float adcVoltageRef)
    : readFn(readFn), readOkFn(readOkFn) {
  this->adcVoltageRef = adcVoltageRef;

  memset(
    &pinValues,
    0,
    sizeof(pinValues[0]) * samples
  );

  temperature = 0;
  valid = false;
  filledSamples = 0;
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
  validity.recordSample(readOkFn());

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }
  int averagedCounts = sum / samples;

  if (filledSamples < (unsigned int)samples) filledSamples++;
  // The moving average is meaningless until the buffer holds `samples` real
  // readings — a partially-filled average (diluted by the zero-filled
  // startup buffer) lands arbitrarily inside or below the valid band and
  // must not be reported as either valid or invalid based on real data.
  valid = (filledSamples >= (unsigned int)samples) &&
          validity.isValid(averagedCounts, NTC_VALID_COUNTS_LOW, NTC_VALID_COUNTS_HIGH);

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
