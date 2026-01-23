#include "TmotorProvider.h"
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

    // Convert decivolts to millivolts: deciVoltage * 100 = milliVolts
    uint16_t deciVoltage = tmotor.getDeciVoltage();
    data->batteryVoltageMilliVolts = (uint32_t)deciVoltage * 100;

    // Convert decicurrent to milliamperes: deciCurrent * 100 = milliAmps
    uint16_t deciCurrent = tmotor.getDeciCurrent();
    data->batteryCurrentMilliAmps = (uint32_t)deciCurrent * 100;

    // RPM is already an integer
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
            .batteryCurrentMilliAmps = 0,
            .rpm = 0,
            .motorTemperatureMilliCelsius = 0,
            .escTemperatureMilliCelsius = 0,
            .lastUpdate = 0
        }
    };

    g_tmotorProvider = &provider;
    return provider;
}

