#ifndef SerialScreen_h
#define SerialScreen_h

#include "../Throttle/Throttle.h"
#include "../Canbus/Canbus.h"
#include "../Temperature/Temperature.h"
#include "../config.h"

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

        void writeBatteryInfo();
        void writeThrottleInfo();
        void writeMotorInfo();
        void writeEscInfo();
        void writeSystemStatus();
};

#endif
