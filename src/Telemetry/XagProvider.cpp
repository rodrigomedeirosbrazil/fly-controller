#include "XagProvider.h"
#include "../config.h"
#include "../Temperature/Temperature.h"

// External temperature sensor for ESC (XAG mode only)
#if IS_XAG
extern Temperature escTemp;
#endif


// Static provider instance
static TelemetryProvider* g_xagProvider = nullptr;

// Static functions for XAG provider
static void xagUpdate() {
    if (!g_xagProvider) return;

    TelemetryData* data = &g_xagProvider->data;

    #if IS_XAG
    // Read battery voltage from ADC using voltage divider
    // Oversampling: take multiple readings and average them
    int oversampledValue = 0;
    const int oversampleCount = 10;
    for (int i = 0; i < oversampleCount; i++) {
        oversampledValue += analogRead(BATTERY_VOLTAGE_PIN);
    }
    int adcValue = oversampledValue / oversampleCount;

    // Convert ADC reading to voltage at GPIO pin
    // ESP32-C3: 12-bit ADC (0-4095) with 3.3V reference
    double voltageAtPin = (ADC_VREF * adcValue) / ADC_MAX_VALUE;

    // Calculate actual battery voltage using divider ratio
    // V_battery = V_pin * BATTERY_DIVIDER_RATIO
    double batteryVoltage = voltageAtPin * BATTERY_DIVIDER_RATIO;

    // Convert to millivolts directly
    // Maximum expected: 60.0V, so 60.0 * 1000 = 60000 mV < 65535 (uint16_t max)
    data->batteryVoltageMilliVolts = (uint16_t)(batteryVoltage * 1000.0 + 0.5);
    #else
    data->batteryVoltageMilliVolts = 0;
    #endif

    // XAG mode: no current data available
    data->batteryCurrent = 0;

    // XAG mode: no RPM data available
    data->rpm = 0;

    // Motor temperature is read separately (motorTemp) - will be updated from main loop

    // Read ESC temperature from NTC sensor
    #if IS_XAG
    float escTempCelsius = escTemp.getTemperature();
    data->escTemperatureMilliCelsius = (int32_t)(escTempCelsius * 1000.0f);
    #endif

    data->lastUpdate = millis();
    data->hasTelemetry = true;
}

static void xagInit() {
    // XAG mode initialization (if needed)
    if (g_xagProvider) {
        g_xagProvider->data.hasTelemetry = true;
    }
}

static TelemetryData* xagGetData() {
    if (!g_xagProvider) return nullptr;
    return &g_xagProvider->data;
}

static void xagSendNodeStatus() {
    // No-op for XAG (no CAN bus)
}

static void xagHandleCanMessage(twai_message_t* msg) {
    // No-op for XAG (no CAN bus)
    (void)msg; // Suppress unused parameter warning
}

static bool xagHasTelemetry() {
    // XAG always has telemetry (data is read from sensors)
    return true;
}

TelemetryProvider createXagProvider() {
    static TelemetryProvider provider = {
        .update = xagUpdate,
        .init = xagInit,
        .getData = xagGetData,
        .sendNodeStatus = xagSendNodeStatus,
        .handleCanMessage = xagHandleCanMessage,
        .hasTelemetry = xagHasTelemetry,
        .data = {
            .hasTelemetry = true,
            .batteryVoltageMilliVolts = 0,
            .batteryCurrent = 0,
            .rpm = 0,
            .motorTemperatureMilliCelsius = 0,
            .escTemperatureMilliCelsius = 0,
            .lastUpdate = 0
        }
    };

    g_xagProvider = &provider;
    return provider;
}

