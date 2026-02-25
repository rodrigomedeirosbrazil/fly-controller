#include "Telemetry.h"
#include "TelemetryBackend.h"
#include "../config_controller.h"
#include "../config.h"

static const TelemetryBackend* s_backend_ptr = nullptr;

#if IS_HOBBYWING
static void wrapUpdate() { hobbywingTelemetry.update(); }
static bool wrapHasData() { return hobbywingTelemetry.hasData(); }
static uint16_t wrapGetBatteryVoltageMilliVolts() { return hobbywingTelemetry.getBatteryVoltageMilliVolts(); }
static uint32_t wrapGetBatteryCurrentMilliAmps() { return hobbywingTelemetry.getBatteryCurrentMilliAmps(); }
static uint16_t wrapGetRpm() { return hobbywingTelemetry.getRpm(); }
static int32_t wrapGetMotorTempMilliCelsius() { return hobbywingTelemetry.getMotorTempMilliCelsius(); }
static int32_t wrapGetEscTempMilliCelsius() { return hobbywingTelemetry.getEscTempMilliCelsius(); }
static unsigned long wrapGetLastUpdate() { return hobbywingTelemetry.getLastUpdate(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate
};
#elif IS_TMOTOR
static void wrapUpdate() { tmotorTelemetry.update(); }
static bool wrapHasData() { return tmotorTelemetry.hasData(); }
static uint16_t wrapGetBatteryVoltageMilliVolts() { return tmotorTelemetry.getBatteryVoltageMilliVolts(); }
static uint32_t wrapGetBatteryCurrentMilliAmps() { return tmotorTelemetry.getBatteryCurrentMilliAmps(); }
static uint16_t wrapGetRpm() { return tmotorTelemetry.getRpm(); }
static int32_t wrapGetMotorTempMilliCelsius() { return tmotorTelemetry.getMotorTempMilliCelsius(); }
static int32_t wrapGetEscTempMilliCelsius() { return tmotorTelemetry.getEscTempMilliCelsius(); }
static unsigned long wrapGetLastUpdate() { return tmotorTelemetry.getLastUpdate(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate
};
#elif IS_XAG
static void wrapUpdate() { xagTelemetry.update(); }
static bool wrapHasData() { return xagTelemetry.hasData(); }
static uint16_t wrapGetBatteryVoltageMilliVolts() { return xagTelemetry.getBatteryVoltageMilliVolts(); }
static uint32_t wrapGetBatteryCurrentMilliAmps() { return xagTelemetry.getBatteryCurrentMilliAmps(); }
static uint16_t wrapGetRpm() { return xagTelemetry.getRpm(); }
static int32_t wrapGetMotorTempMilliCelsius() { return xagTelemetry.getMotorTempMilliCelsius(); }
static int32_t wrapGetEscTempMilliCelsius() { return xagTelemetry.getEscTempMilliCelsius(); }
static unsigned long wrapGetLastUpdate() { return xagTelemetry.getLastUpdate(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate
};
#endif

void Telemetry::init() {
#if IS_HOBBYWING || IS_TMOTOR || IS_XAG
    s_backend_ptr = &s_backend;
#endif
}

void Telemetry::update() {
    if (s_backend_ptr && s_backend_ptr->update) {
        s_backend_ptr->update();
    }
}

bool Telemetry::hasData() const {
    return s_backend_ptr && s_backend_ptr->hasData && s_backend_ptr->hasData();
}

uint16_t Telemetry::getBatteryVoltageMilliVolts() const {
    return s_backend_ptr && s_backend_ptr->getBatteryVoltageMilliVolts ? s_backend_ptr->getBatteryVoltageMilliVolts() : 0;
}

uint32_t Telemetry::getBatteryCurrentMilliAmps() const {
    return s_backend_ptr && s_backend_ptr->getBatteryCurrentMilliAmps ? s_backend_ptr->getBatteryCurrentMilliAmps() : 0;
}

uint16_t Telemetry::getRpm() const {
    return s_backend_ptr && s_backend_ptr->getRpm ? s_backend_ptr->getRpm() : 0;
}

int32_t Telemetry::getMotorTempMilliCelsius() const {
    return s_backend_ptr && s_backend_ptr->getMotorTempMilliCelsius ? s_backend_ptr->getMotorTempMilliCelsius() : 0;
}

int32_t Telemetry::getEscTempMilliCelsius() const {
    return s_backend_ptr && s_backend_ptr->getEscTempMilliCelsius ? s_backend_ptr->getEscTempMilliCelsius() : 0;
}

unsigned long Telemetry::getLastUpdate() const {
    return s_backend_ptr && s_backend_ptr->getLastUpdate ? s_backend_ptr->getLastUpdate() : 0;
}

Telemetry telemetry;
