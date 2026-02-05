#include "ADS1115.h"

ADS1115::ADS1115() {
    initialized = false;
    for (int i = 0; i < 4; i++) {
        lastValue[i] = 0;
    }
}

bool ADS1115::begin(uint8_t sdaPin, uint8_t sclPin) {
    Wire.begin(sdaPin, sclPin);

    // Initialize ADS1115 with default address (0x48)
    if (!ads.begin()) {
        Serial.println("[ADS1115] ERROR: Failed to initialize ADS1115");
        initialized = false;
        return false;
    }

    // Set gain to ±4.096V (default, suitable for 0-3.3V sensors)
    // This gives us full scale of ±4.096V, which covers our 0-3.3V range
    ads.setGain(GAIN_TWOTHIRDS); // ±6.144V range (default)
    // Actually, let's use GAIN_ONE for ±4.096V which is closer to our 3.3V reference
    ads.setGain(GAIN_ONE); // ±4.096V range

    initialized = true;
    Serial.println("[ADS1115] Initialized successfully");

    // Test read to verify communication
    int16_t testValue = ads.readADC_SingleEnded(0);
    Serial.print("[ADS1115] Test read channel 0: ");
    Serial.println(testValue);

    return true;
}

int ADS1115::readChannel(uint8_t channel) {
    if (!initialized) {
        Serial.println("[ADS1115] ERROR: Not initialized");
        return lastValue[channel];
    }

    if (channel > 3) {
        Serial.print("[ADS1115] ERROR: Invalid channel ");
        Serial.println(channel);
        return lastValue[channel];
    }

    int16_t rawValue = ads.readADC_SingleEnded(channel);

    // Handle error case (negative values can indicate errors in some cases)
    // But ADS1115 can return negative values for differential readings
    // For single-ended, values should be 0-32767, but let's be safe
    if (rawValue < 0) {
        // If we get a negative value in single-ended mode, use last valid value
        Serial.print("[ADS1115] WARNING: Negative value on channel ");
        Serial.print(channel);
        Serial.print(", using last value: ");
        Serial.println(lastValue[channel]);
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

