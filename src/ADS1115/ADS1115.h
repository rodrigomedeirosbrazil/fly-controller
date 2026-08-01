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

        // True if the most recent readChannel() call completed a real I2C
        // transaction; false if the bus was unresponsive and a cached value
        // was returned instead. Shared across channels — reflects overall bus
        // health at the time of the last read, not a per-channel state.
        // Callers that need the health of a specific channel's read must call
        // this immediately after that channel's readChannel(), before any
        // other channel is read.
        bool lastReadOk() const { return lastReadOk_; }

    private:
        Adafruit_ADS1115 ads;
        bool initialized;
        bool lastReadOk_;
        int lastValue[4]; // Store last valid value for each channel (0-3)
        double lastVoltage[4]; // Store last valid voltage for each channel (0-3)

        // Convert 16-bit ADS1115 value (0-32767) to 12-bit equivalent (0-4095)
        int convertTo12Bit(int adsValue);

        // Cheap I2C presence probe. Wire::endTransmission() has its own
        // internal bus timeout, unlike Adafruit_ADS1X15::readADC_SingleEnded's
        // conversion-ready wait (`while (!conversionComplete());`), which has
        // none. Probing first means a wedged bus returns false here instead of
        // hanging loop() inside the library call.
        bool probeAck();

        // ADS1115 reference voltage for GAIN_ONE
        static constexpr double ADS1115_VREF = 4.096; // Volts
        static constexpr int ADS1115_MAX_VALUE = 32767; // 16-bit max value
};

#endif

