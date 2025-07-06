#ifndef SerialScreen_h
#define SerialScreen_h

#include "../config.h"

class Throttle;
class Canbus;
class Temperature;

class SerialScreen
{
    public:
        SerialScreen();
        void init(unsigned long baudRate = 9600);
        void write();

    private:
        unsigned long lastSerialUpdate;

        void clearScreen();
        void writeBatteryInfo();
        void writeThrottleInfo();
        void writeMotorInfo();
        void writeEscInfo();
        void writeSystemStatus();
        void writeCalibrationInfo();
};

#endif
