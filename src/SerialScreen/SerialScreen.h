#ifndef SerialScreen_h
#define SerialScreen_h

#include "../config.h"

class Throttle;
class Canbus;
class Temperature;

class SerialScreen
{
    public:
        SerialScreen(
            Throttle* throttle,
            Canbus* canbus,
            Temperature* motorTemp
        );
        void init(unsigned long baudRate = 9600);
        void write();

    private:
        Throttle* throttle;
        Canbus* canbus;
        Temperature* motorTemp;

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
