#ifndef XCTOD_H
#define XCTOD_H

#include <Arduino.h>

class Throttle;
class Canbus;
class Hobbywing;
class Temperature;
class Power;

class Xctod {
public:
    Xctod();

    void init(unsigned long baudRate = 9600);
    void write();

private:
    unsigned long lastSerialUpdate;
    static const unsigned long UPDATE_INTERVAL = 1000; // Update every 1 second

    void writeBatteryInfo();
    void writeThrottleInfo();
    void writeMotorInfo();
    void writeEscInfo();
    void writeSystemStatus();
};

#endif // XCTOD_H
