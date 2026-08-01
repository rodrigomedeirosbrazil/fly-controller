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

        // True if the most recent readChannel(channel) call for this specific
        // channel completed a real I2C transaction within the conversion
        // timeout and returned a value ADS1115 could plausibly produce.
        // Tracked per channel — safe to call at any point after that
        // channel's own readChannel(), regardless of what other channels
        // were read in between.
        bool lastReadOk(uint8_t channel) const {
            if (channel > 3) return false;
            return lastReadOk_[channel];
        }

    private:
        Adafruit_ADS1115 ads;
        bool initialized;
        bool lastReadOk_[4];
        int lastValue[4]; // Store last valid value for each channel (0-3)
        double lastVoltage[4]; // Store last valid voltage for each channel (0-3)

        // Convert 16-bit ADS1115 value (0-32767) to 12-bit equivalent (0-4095)
        int convertTo12Bit(int adsValue);

        // Cheap first-pass I2C presence check — catches an already-dead bus
        // before spending time starting a conversion. readChannel() also
        // bounds the conversion-ready wait itself (see kConversionTimeoutMs
        // in the .cpp), since a bus that ACKs here can still wedge
        // mid-conversion; the probe narrows the window, it doesn't replace
        // the bound.
        bool probeAck();

        // ADS1115 reference voltage for GAIN_ONE
        static constexpr double ADS1115_VREF = 4.096; // Volts
        static constexpr int ADS1115_MAX_VALUE = 32767; // 16-bit max value
};

#endif

