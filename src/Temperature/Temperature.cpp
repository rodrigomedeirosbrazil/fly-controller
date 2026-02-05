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
  // ADS1115 is more precise, so we use less oversampling for it
  int oversampledValue = 0;
#if IS_TMOTOR
  // Use ADS1115 for Tmotor - single read is sufficient (ADS1115 is 16-bit, more precise)
  oversampledValue = ads1115.readChannel(ADS1115_TEMP_CHANNEL);
#else
  // Use built-in ADC for Hobbywing and XAG - multiple reads needed for noise reduction
  for (int i = 0; i < oversample; i++) {
    oversampledValue += analogRead(pin);
  }
  oversampledValue = oversampledValue / oversample;
#endif
  pinValues[samples - 1] = oversampledValue;

  // Calculate moving average
  int sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += pinValues[i];
  }

  double v; // Voltage at the divider point
#if IS_TMOTOR
  // ADS1115: Calculate voltage from averaged ADC values using correct reference
  // ADS1115 uses 4.096V reference (GAIN_ONE), but values are converted to 0-4095 scale
  // To get accurate voltage, we need to convert back: v = (value * 4.096) / 4095
  // But wait, the conversion was: value = (adsRaw * 4095) / 32767
  // So: adsRaw = (value * 32767) / 4095
  // And: v = (adsRaw * 4.096) / 32767 = (value * 4.096) / 4095
  // However, since the sensor is powered by 3.3V, the max reading will be:
  // maxValue = (3.3/4.096) * 4095 ≈ 3300
  // So we should use: v = (sum * 4.096) / (samples * 4095)
  // But to match the actual sensor voltage (3.3V max), we use:
  v = (sum * 4.096) / (samples * 4095);
#else
  // ESP32-C3: 12-bit ADC (0-4095) with 3.3V reference
  // Divider: 3.3V -> [R 10K] -> GPIO1 -> [NTC rt] -> GND
  // Voltage at GPIO1: v = ADC_VREF * rt / (R + rt)
  v = (ADC_VREF * sum) / (samples * ADC_MAX_VALUE);
#endif

  // Solving for rt: rt = (v * R) / (VCC - v)
  // VCC is 3.3V (sensor power supply), not the ADC reference
  double rt = (v * R) / (ADC_VREF - v);

  // Steinhart-Hart equation using Beta coefficient:
  // 1/T = 1/T0 + (1/B) * ln(Rt/R0)
  // T = 1 / (1/T0 + (1/B) * ln(Rt/R0))
  double invT = (1.0 / t0) + (1.0 / beta) * log(rt / r0);
  double tempK = 1.0 / invT;
  temperature = tempK - 273.15;
}
