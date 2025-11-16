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
        bool isCalibrated() { return calibrated; }

        unsigned int getThrottlePercentage();
        unsigned int getThrottleRaw();
        unsigned int getThrottlePinMin() { return throttlePinMin; }
        unsigned int getThrottlePinMax() { return throttlePinMax; }
        int getPinValueFiltered() { return pinValueFiltered; }
        unsigned int getCalibratingStep() { return calibratingStep; }

    private:
        const static int FILTER_SAMPLE_COUNT = 9;
        const unsigned long SAMPLE_INTERVAL_US = 100;
        const unsigned int calibrationTime = 3000; // 3 seconds for calibration
        const int calibrationThreshold = 2000; // Threshold for detecting throttle movement

        int pinValueFiltered;

        int filterSamples[FILTER_SAMPLE_COUNT];
        int sampleCount;
        unsigned long lastSampleTimeMicros;

        bool throttleArmed;

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

        void resetCalibration();
        void handleCalibration(unsigned long now);
};

#endif
