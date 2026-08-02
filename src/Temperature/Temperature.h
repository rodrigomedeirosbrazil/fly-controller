#ifndef Temperature_h
#define Temperature_h

#include "../ADS1115/SensorReadingValidity.h"

class Temperature
{
    public:
        typedef int (*ReadFn)();
        typedef bool (*ReadOkFn)();
        Temperature(ReadFn readFn, ReadOkFn readOkFn, float adcVoltageRef);
        void handle();
        double getTemperature() { return temperature; }
        bool isValid() const { return valid; }

    private:
        const double beta = 3600.0;
        const double r0 = 10000.0; // Resistance at T0
        const double t0 = 298.15;   // 25°C in Kelvin
        const double R = 10000.0;
        const static int samples = 10;

        // NTC divider valid band in raw ADC counts (0-4095), corresponding
        // to -20..150°C. See
        // docs/superpowers/specs/2026-08-01-signal-validity-design.md,
        // "Detection per source". Checked in the count domain, before the
        // Steinhart-Hart math, so a disconnected (reads near 3299 — VCC is
        // 3.3V against the ADS1115's 4.096V reference, so the tap can never
        // reach full scale) or shorted (reads near 0) sensor is flagged
        // here so consumers never use the NaN/garbage value the math below
        // can still produce internally on a disconnected sensor.
        enum : int { NTC_VALID_COUNTS_LOW = 91, NTC_VALID_COUNTS_HIGH = 2954 }; // 150°C, -20°C

        ReadFn readFn;
        ReadOkFn readOkFn;
        float adcVoltageRef;
        int pinValues[samples];
        double temperature;
        bool valid;
        unsigned int filledSamples; // how many real readTemperature() ticks have run; valid stays false until this reaches `samples`
        SensorReadingValidity validity;
        unsigned long lastPinRead;

        void readTemperature();
};

#endif
