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

    if (!tmotor.hasTelemetry()) {
        data->hasTelemetry = false;
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
    data->hasTelemetry = true;
}

static void tmotorInit() {
    // Tmotor initialization is done in main.cpp
    if (g_tmotorProvider) {
        g_tmotorProvider->data.hasTelemetry = false; // Will be set to true when data arrives
    }
}

static TelemetryData* tmotorGetData() {
    if (!g_tmotorProvider) return nullptr;
    return &g_tmotorProvider->data;
}

static void tmotorSendNodeStatus() {
    extern Canbus canbus;
    canbus.sendNodeStatus();
}

static void tmotorHandleCanMessage(twai_message_t* msg) {
    tmotor.parseEscMessage(msg);
}

static bool tmotorHasTelemetry() {
    return tmotor.hasTelemetry();
}

TelemetryProvider createTmotorProvider() {
    static TelemetryProvider provider = {
        .update = tmotorUpdate,
        .init = tmotorInit,
        .getData = tmotorGetData,
        .sendNodeStatus = tmotorSendNodeStatus,
        .handleCanMessage = tmotorHandleCanMessage,
        .hasTelemetry = tmotorHasTelemetry,
        .data = {
            .hasTelemetry = false,
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
        .sendNodeStatus = nullptr,
        .handleCanMessage = nullptr,
        .hasTelemetry = nullptr,
        .data = {}
    };
    return provider;
}
#endif

