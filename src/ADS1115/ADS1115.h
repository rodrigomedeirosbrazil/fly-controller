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
        bool isReady() { return initialized; }

    private:
        Adafruit_ADS1115 ads;
        bool initialized;
        int lastValue[4]; // Store last valid value for each channel (0-3)

        // Convert 16-bit ADS1115 value (0-32767) to 12-bit equivalent (0-4095)
        int convertTo12Bit(int adsValue);
};

#endif

