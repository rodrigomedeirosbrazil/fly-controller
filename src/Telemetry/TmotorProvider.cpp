#include "TmotorProvider.h"
#include "../config_controller.h"
#if IS_TMOTOR
#include "../config.h"
#include "../Tmotor/Tmotor.h"

// Forward declarations
extern Tmotor tmotor;

// Static provider instance
static TelemetryProvider* g_tmotorProvider = nullptr;

// Static functions for Tmotor provider
static void tmotorUpdate() {
    if (!g_tmotorProvider) return;

    TelemetryData* data = &g_tmotorProvider->data;

    if (!tmotor.isReady()) {
        data->isReady = false;
        return;
    }

    data->batteryVoltageMilliVolts = tmotor.getBatteryVoltageMilliVolts();
    data->batteryCurrent = tmotor.getBatteryCurrent();

    data->rpm = tmotor.getRpm();

    // Convert motor temperature (uint8_t Celsius) to millicelsius: temp * 1000
    uint8_t motorTempCelsius = tmotor.getMotorTemperature();
    data->motorTemperatureMilliCelsius = (int32_t)motorTempCelsius * 1000;

    // Convert ESC temperature (uint8_t Celsius) to millicelsius: temp * 1000
    uint8_t escTempCelsius = tmotor.getEscTemperature();
    data->escTemperatureMilliCelsius = (int32_t)escTempCelsius * 1000;

    data->lastUpdate = millis();
    data->isReady = true;
}

static void tmotorInit() {
    // Tmotor initialization is done in main.cpp
    if (g_tmotorProvider) {
        g_tmotorProvider->data.isReady = false; // Will be set to true when data arrives
    }
}

static TelemetryData* tmotorGetData() {
    if (!g_tmotorProvider) return nullptr;
    return &g_tmotorProvider->data;
}

static void tmotorAnnounce() {
    tmotor.announce();
}

static void tmotorHandleCanMessage(twai_message_t* msg) {
    tmotor.parseEscMessage(msg);
}

static bool tmotorIsReady() {
    return tmotor.isReady();
}

TelemetryProvider createTmotorProvider() {
    static TelemetryProvider provider = {
        .update = tmotorUpdate,
        .init = tmotorInit,
        .getData = tmotorGetData,
        .announce = tmotorAnnounce,
        .handleCanMessage = tmotorHandleCanMessage,
        .isReady = tmotorIsReady,
        .data = {
            .isReady = false,
            .batteryVoltageMilliVolts = 0,
            .batteryCurrent = 0,
            .rpm = 0,
            .motorTemperatureMilliCelsius = 0,
            .escTemperatureMilliCelsius = 0,
            .lastUpdate = 0
        }
    };

    g_tmotorProvider = &provider;
    return provider;
}
#else
// Dummy implementation when not using Tmotor
TelemetryProvider createTmotorProvider() {
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

