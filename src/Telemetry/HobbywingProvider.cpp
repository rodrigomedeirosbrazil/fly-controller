#include "HobbywingProvider.h"
#include "../config_controller.h"
#if IS_HOBBYWING
#include "../config.h"
#include "../Hobbywing/Hobbywing.h"

// Forward declarations
extern Hobbywing hobbywing;

// Static provider instance
static TelemetryProvider* g_hobbywingProvider = nullptr;

// Static functions for Hobbywing provider
static void hobbywingUpdate() {
    if (!g_hobbywingProvider) return;

    TelemetryData* data = &g_hobbywingProvider->data;

    if (!hobbywing.hasTelemetry()) {
        data->hasTelemetry = false;
        return;
    }

    // Voltage is already in millivolts from Hobbywing class
    data->batteryVoltageMilliVolts = hobbywing.getBatteryVoltageMilliVolts();

    data->batteryCurrentMilliAmps = hobbywing.getBatteryCurrent();

    data->rpm = hobbywing.getRpm();

    // Motor temperature is read separately (motorTemp) - will be updated from main loop

    // Convert temperature (uint8_t Celsius) to millicelsius: temp * 1000
    uint8_t escTempCelsius = hobbywing.getTemperature();
    data->escTemperatureMilliCelsius = (int32_t)escTempCelsius * 1000;

    data->lastUpdate = millis();
    data->hasTelemetry = true;
}

static void hobbywingInit() {
    // Hobbywing initialization is done in main.cpp
    if (g_hobbywingProvider) {
        g_hobbywingProvider->data.hasTelemetry = false; // Will be set to true when data arrives
    }
}

static TelemetryData* hobbywingGetData() {
    if (!g_hobbywingProvider) return nullptr;
    return &g_hobbywingProvider->data;
}

static void hobbywingSendNodeStatus() {
    extern Canbus canbus;
    canbus.sendNodeStatus();
}

static void hobbywingHandleCanMessage(twai_message_t* msg) {
    hobbywing.parseEscMessage(msg);
}

static bool hobbywingHasTelemetry() {
    return hobbywing.hasTelemetry();
}

TelemetryProvider createHobbywingProvider() {
    static TelemetryProvider provider = {
        .update = hobbywingUpdate,
        .init = hobbywingInit,
        .getData = hobbywingGetData,
        .sendNodeStatus = hobbywingSendNodeStatus,
        .handleCanMessage = hobbywingHandleCanMessage,
        .hasTelemetry = hobbywingHasTelemetry,
        .data = {
            .hasTelemetry = false,
            .batteryVoltageMilliVolts = 0,
            .batteryCurrentMilliAmps = 0,
            .rpm = 0,
            .motorTemperatureMilliCelsius = 0,
            .escTemperatureMilliCelsius = 0,
            .lastUpdate = 0
        }
    };

    g_hobbywingProvider = &provider;
    return provider;
}
#else
// Dummy implementation when not using Hobbywing
TelemetryProvider createHobbywingProvider() {
    static TelemetryProvider provider = {
        .update = nullptr,
        .init = nullptr,
        .getData = nullptr,
        .sendNodeStatus = nullptr,
        .handleCanMessage = nullptr,
        .hasTelemetry = nullptr,
        .data = {}
    };
    return provider;
}
#endif

