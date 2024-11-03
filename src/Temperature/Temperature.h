#ifndef Temperature_h
#define Temperature_h

#include "../config.h"

const double beta = 3600.0;
const double r0 = 10000.0;
const double t0 = 273.0 + 25.0;
const double rx = r0 * exp(-beta/t0);

const double vcc = 5.0;
const double R = 10000.0;

class Temperature
{
    public:
        Temperature();
        void tick();
        double getTemperature() { return temperature; }

    private:
        int pinValues[SAMPLES_FOR_FILTER];
        double temperature;
        unsigned long lastPinRead;

        void readTemperature();
};

#endif