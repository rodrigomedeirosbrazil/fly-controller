#ifndef Throttle_h
#define Throttle_h

#include "../Buzzer/Buzzer.h"
#include "../config.h"

class Throttle {
    public:
        Throttle();
        void handle();
        bool isArmed() { return throttleArmed; }
        void setArmed();
        void setDisarmed();
        bool isCruising() { return cruising; }
        bool isCalibrated() { return calibrated; }
        void setCruising(int throttlePosition);
        void cancelCruise();

        unsigned int getThrottlePercentage();
        unsigned int getThrottleRaw();
        int getThrottlePinMin() { return throttlePinMin; }
        int getThrottlePinMax() { return throttlePinMax; }
        unsigned int getCruisingThrottlePosition() { return cruisingThrottlePosition; }
        int getPinValueFiltered() { return pinValueFiltered; }
        unsigned int getCalibratingStep() { return calibratingStep; }

    private:
        const unsigned int timeToBeOnCruising = 30000;
        const unsigned int throttleRange = 5;
        const unsigned int minCrusingThrottle = 30;
        const static int samples = 5;
        const unsigned int calibrationTime = 3000; // 3 seconds for calibration
        const int calibrationThreshold = 500; // Threshold for detecting throttle movement

        int pinValues[samples];
        int pinValueFiltered;
        unsigned long lastThrottleRead;

        bool throttleArmed;
        unsigned int cruising;
        unsigned int cruisingThrottlePosition;
        unsigned int lastThrottlePosition;
        unsigned long timeThrottlePosition;

        bool calibrated;
        unsigned int calibratingStep;
        unsigned long calibrationStartTime;
        int calibrationMaxValue;
        int calibrationMinValue;
        unsigned long calibrationSumMax;
        unsigned int calibrationCountMax;
        unsigned long calibrationSumMin;
        unsigned int calibrationCountMin;

        unsigned int armingTries;

        int throttlePinMin;
        int throttlePinMax;

        void readThrottlePin();
        void checkIfChangedCruiseState();
        void resetCalibration();
        void handleCalibration(unsigned long now);
};

#endif
