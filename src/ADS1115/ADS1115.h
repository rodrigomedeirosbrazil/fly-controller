#ifndef ADS1115_H
#define ADS1115_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

class ADS1115 {
    public:
        ADS1115();
        bool begin(uint8_t sdaPin, uint8_t sclPin);
        int readChannel(uint8_t channel);
        double readVoltage(uint8_t channel); // Read voltage directly (for accurate temperature calculation)
        bool isReady() { return initialized; }

    private:
        Adafruit_ADS1115 ads;
        bool initialized;
        int lastValue[4]; // Store last valid value for each channel (0-3)
        double lastVoltage[4]; // Store last valid voltage for each channel (0-3)

        // Convert 16-bit ADS1115 value (0-32767) to 12-bit equivalent (0-4095)
        int convertTo12Bit(int adsValue);

        // ADS1115 reference voltage for GAIN_ONE
        static constexpr double ADS1115_VREF = 4.096; // Volts
        static constexpr int ADS1115_MAX_VALUE = 32767; // 16-bit max value
};

#endif

