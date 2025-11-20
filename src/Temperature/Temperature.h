#ifndef Temperature_h
#define Temperature_h

#include "../config.h"

class Temperature
{
    public:
        Temperature(uint8_t pin);
        void handle();
        double getTemperature() { return temperature; }

    private:
        const double beta = 3600.0;
        const double r0 = 10000.0; // Resistance at T0
        const double t0 = 298.15;   // 25°C in Kelvin
        const double R = 10000.0;
        const static int samples = 10;
        const static int oversample = 4; // Number of readings to average per sample

        uint8_t pin;
        int pinValues[samples];
        double temperature;
        unsigned long lastPinRead;

        void readTemperature();
};

#endif
