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

  // Oversampling: take multiple readings and average them
  // This reduces random noise from the ADC
  int oversampledValue = 0;
  for (int i = 0; i < oversample; i++) {
#if IS_TMOTOR
    // Use ADS1115 for Tmotor
    oversampledValue += ads1115.readChannel(ADS1115_TEMP_CHANNEL);
#else
    // Use built-in ADC for Hobbywing and XAG
    oversampledValue += analogRead(pin);
#endif
  }
  pinValues[samples - 1] = oversampledValue / oversample;

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  // ESP32-C3: 12-bit ADC (0-4095) with 3.3V reference
  // Divider: 3.3V -> [R 10K] -> GPIO1 -> [NTC rt] -> GND
  // Voltage at GPIO1: v = ADC_VREF * rt / (R + rt)
  // Solving for rt: rt = (v * R) / (ADC_VREF - v)
  // Note: ADS1115 values are already converted to 12-bit equivalent (0-4095)
  double v = (ADC_VREF * sum) / (samples * ADC_MAX_VALUE);
  double rt = (v * R) / (ADC_VREF - v);

  // Steinhart-Hart equation using Beta coefficient:
  // 1/T = 1/T0 + (1/B) * ln(Rt/R0)
  // T = 1 / (1/T0 + (1/B) * ln(Rt/R0))
  double invT = (1.0 / t0) + (1.0 / beta) * log(rt / r0);
  double tempK = 1.0 / invT;
  temperature = tempK - 273.15;
}
