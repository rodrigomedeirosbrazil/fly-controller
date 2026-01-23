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

    if (!hobbywing.isReady()) {
        data->isReady = false;
        return;
    }

    // Convert decivolts to millivolts: deciVolts * 100 = milliVolts
    uint16_t deciVolts = hobbywing.getDeciVoltage();
    data->batteryVoltageMilliVolts = (uint32_t)deciVolts * 100;

    // Convert decicurrent to milliamperes: deciCurrent * 100 = milliAmps
    uint16_t deciCurrent = hobbywing.getDeciCurrent();
    data->batteryCurrentMilliAmps = (uint32_t)deciCurrent * 100;

    // RPM is already an integer
    data->rpm = hobbywing.getRpm();

    // Motor temperature is read separately (motorTemp) - will be updated from main loop

    // Convert temperature (uint8_t Celsius) to millicelsius: temp * 1000
    uint8_t escTempCelsius = hobbywing.getTemperature();
    data->escTemperatureMilliCelsius = (int32_t)escTempCelsius * 1000;

    data->lastUpdate = millis();
    data->isReady = true;
}

static void hobbywingInit() {
    // Hobbywing initialization is done in main.cpp
    if (g_hobbywingProvider) {
        g_hobbywingProvider->data.isReady = false; // Will be set to true when data arrives
    }
}

static TelemetryData* hobbywingGetData() {
    if (!g_hobbywingProvider) return nullptr;
    return &g_hobbywingProvider->data;
}

static void hobbywingAnnounce() {
    hobbywing.announce();
}

static void hobbywingHandleCanMessage(twai_message_t* msg) {
    hobbywing.parseEscMessage(msg);
}

static bool hobbywingIsReady() {
    return hobbywing.isReady();
}

TelemetryProvider createHobbywingProvider() {
    static TelemetryProvider provider = {
        .update = hobbywingUpdate,
        .init = hobbywingInit,
        .getData = hobbywingGetData,
        .announce = hobbywingAnnounce,
        .handleCanMessage = hobbywingHandleCanMessage,
        .isReady = hobbywingIsReady,
        .data = {
            .isReady = false,
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
        .announce = nullptr,
        .handleCanMessage = nullptr,
        .isReady = nullptr,
        .data = {}
    };
    return provider;
}
#endif

