#include "ADS1115.h"

// Mux config value for each single-ended channel — mirrors Adafruit_ADS1X15's
// internal (private) MUX_BY_CHANNEL table, which isn't exposed to callers.
static const uint16_t kMuxByChannel[4] = {
    ADS1X15_REG_CONFIG_MUX_SINGLE_0,
    ADS1X15_REG_CONFIG_MUX_SINGLE_1,
    ADS1X15_REG_CONFIG_MUX_SINGLE_2,
    ADS1X15_REG_CONFIG_MUX_SINGLE_3,
};

// At 860 SPS one conversion takes ~1.2ms (see the setDataRate() comment in
// begin()); 10ms gives ~8x margin for interrupt jitter while still bounding
// the wait — this replaces the library's own conversionComplete() busy-wait,
// which has no timeout at all.
static const uint32_t kConversionTimeoutMs = 10;

ADS1115::ADS1115() {
    initialized = false;
    for (int i = 0; i < 4; i++) {
        lastReadOk_[i] = false;
        lastValue[i] = 0;
        lastVoltage[i] = 0.0;
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
    if (channel > 3) {
        return 0;  // Invalid channel — array has only 4 elements [0..3]
    }

    if (!initialized) {
        lastReadOk_[channel] = false;
        return lastValue[channel];
    }

    if (!probeAck()) {
        // Bus is unresponsive — do not call into the library. Its
        // conversion-ready wait has no timeout and would hang loop().
        lastReadOk_[channel] = false;
        return lastValue[channel];
    }

    ads.startADCReading(kMuxByChannel[channel], /*continuous=*/false);

    uint32_t conversionStart = millis();
    while (!ads.conversionComplete()) {
        if (millis() - conversionStart >= kConversionTimeoutMs) {
            lastReadOk_[channel] = false;
            return lastValue[channel];
        }
    }

    int16_t rawValue = ads.getLastConversionResults();

    // Handle error case (negative values can indicate errors in some cases)
    // But ADS1115 can return negative values for differential readings
    // For single-ended, values should be 0-32767, but let's be safe
    if (rawValue < 0) {
        // If we get a negative value in single-ended mode, use last valid value
        lastReadOk_[channel] = false;
        return lastValue[channel];
    }

    lastReadOk_[channel] = true;

    // Calculate voltage from raw value (for accurate calculations)
    double voltage = (rawValue * ADS1115_VREF) / ADS1115_MAX_VALUE;
    lastVoltage[channel] = voltage;

    // Convert to 12-bit equivalent (0-4095) for compatibility with existing code
    int convertedValue = convertTo12Bit(rawValue);

    // Store last valid value
    lastValue[channel] = convertedValue;

    return convertedValue;
}

double ADS1115::readVoltage(uint8_t channel) {
    if (!initialized) {
        return lastVoltage[channel];
    }

    if (channel > 3) {
        return lastVoltage[channel];
    }

    // Read channel to update voltage cache
    readChannel(channel);

    return lastVoltage[channel];
}

bool ADS1115::probeAck() {
    Wire.beginTransmission(ADS1X15_ADDRESS);
    return Wire.endTransmission() == 0;
}

int ADS1115::convertTo12Bit(int adsValue) {
    // ADS1115: 16-bit (0-32767) with ±4.096V range
    // ESP32-C3 ADC: 12-bit (0-4095) with 3.3V reference
    // Convert: adcValue = (ads1115Value * 4095) / 32767
    // This maintains the same relative scale
    return (adsValue * 4095) / 32767;
}

