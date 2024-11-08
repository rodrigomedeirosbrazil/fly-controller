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
        const double r0 = 10000.0;
        const double t0 = 273.0 + 25.0;
        const double rx = r0 * exp(-beta / t0);
        const double vcc = 5.0;
        const double R = 10000.0;
        const static int samples = 5;

        uint8_t pin;
        int pinValues[samples];
        double temperature;
        unsigned long lastPinRead;

        void readTemperature();
};

#endif