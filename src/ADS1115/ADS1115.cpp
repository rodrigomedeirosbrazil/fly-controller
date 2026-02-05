#include "ADS1115.h"

ADS1115::ADS1115() {
    initialized = false;
    for (int i = 0; i < 4; i++) {
        lastValue[i] = 0;
    }
}

bool ADS1115::begin(uint8_t sdaPin, uint8_t sclPin) {
    // Configure I2C for high speed (400kHz) for faster communication
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(400000); // 400kHz I2C speed

    // Initialize ADS1115 with default address (0x48)
    if (!ads.begin()) {
        initialized = false;
        return false;
    }

    // Set gain to ±4.096V (suitable for 0-3.3V sensors)
    ads.setGain(GAIN_ONE); // ±4.096V range

    // Set data rate to 860 SPS (Samples Per Second) for fastest conversion
    // This reduces conversion time from ~8ms to ~1.2ms per reading
    ads.setDataRate(RATE_ADS1115_860SPS);

    initialized = true;

    return true;
}

int ADS1115::readChannel(uint8_t channel) {
    if (!initialized) {
        return lastValue[channel];
    }

    if (channel > 3) {
        return lastValue[channel];
    }

    int16_t rawValue = ads.readADC_SingleEnded(channel);

    // Handle error case (negative values can indicate errors in some cases)
    // But ADS1115 can return negative values for differential readings
    // For single-ended, values should be 0-32767, but let's be safe
    if (rawValue < 0) {
        // If we get a negative value in single-ended mode, use last valid value
        return lastValue[channel];
    }

    // Convert to 12-bit equivalent (0-4095) for compatibility with existing code
    int convertedValue = convertTo12Bit(rawValue);

    // Store last valid value
    lastValue[channel] = convertedValue;

    return convertedValue;
}

int ADS1115::convertTo12Bit(int adsValue) {
    // ADS1115: 16-bit (0-32767) with ±4.096V range
    // ESP32-C3 ADC: 12-bit (0-4095) with 3.3V reference
    // Convert: adcValue = (ads1115Value * 4095) / 32767
    // This maintains the same relative scale
    return (adsValue * 4095) / 32767;
}

