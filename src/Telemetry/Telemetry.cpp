#include "Telemetry.h"
#include "TelemetryBackend.h"
#include "../config_controller.h"
#include "../config.h"

static const TelemetryBackend* s_backend_ptr = nullptr;

#if IS_TMOTOR
static void wrapUpdate() { tmotorTelemetry.update(); }
static bool wrapHasData() { return tmotorTelemetry.hasData(); }
static uint16_t wrapGetBatteryVoltageMilliVolts() { return tmotorTelemetry.getBatteryVoltageMilliVolts(); }
static uint32_t wrapGetBatteryCurrentMilliAmps() { return tmotorTelemetry.getBatteryCurrentMilliAmps(); }
static uint16_t wrapGetRpm() { return tmotorTelemetry.getRpm(); }
static int32_t wrapGetMotorTempMilliCelsius() { return tmotorTelemetry.getMotorTempMilliCelsius(); }
static int32_t wrapGetEscTempMilliCelsius() { return tmotorTelemetry.getEscTempMilliCelsius(); }
static unsigned long wrapGetLastUpdate() { return tmotorTelemetry.getLastUpdate(); }
static SignalState wrapGetMotorTempState() { return tmotorTelemetry.getMotorTempState(); }
static SignalState wrapGetEscTempState() { return tmotorTelemetry.getEscTempState(); }
static SignalState wrapGetBatteryVoltageState() { return tmotorTelemetry.getBatteryVoltageState(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate,
    wrapGetMotorTempState, wrapGetEscTempState, wrapGetBatteryVoltageState
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
static SignalState wrapGetMotorTempState() { return xagTelemetry.getMotorTempState(); }
static SignalState wrapGetEscTempState() { return xagTelemetry.getEscTempState(); }
static SignalState wrapGetBatteryVoltageState() { return xagTelemetry.getBatteryVoltageState(); }
static const TelemetryBackend s_backend = {
    wrapUpdate, wrapHasData, wrapGetBatteryVoltageMilliVolts, wrapGetBatteryCurrentMilliAmps,
    wrapGetRpm, wrapGetMotorTempMilliCelsius, wrapGetEscTempMilliCelsius, wrapGetLastUpdate,
    wrapGetMotorTempState, wrapGetEscTempState, wrapGetBatteryVoltageState
};
#endif

void Telemetry::init() {
#if IS_TMOTOR || IS_XAG
    s_backend_ptr = &s_backend;
#endif
}

void Telemetry::update() {
    if (s_backend_ptr && s_backend_ptr->update) {
        s_backend_ptr->update();
    }
}

bool Telemetry::hasData() const {
    if (s_backend_ptr && s_backend_ptr->hasData && s_backend_ptr->hasData()) {
        return true;
    }
    return bluetoothBms.hasData();
}

uint16_t Telemetry::getBatteryVoltageMilliVolts() const {
    uint16_t v = s_backend_ptr && s_backend_ptr->getBatteryVoltageMilliVolts ? s_backend_ptr->getBatteryVoltageMilliVolts() : 0;
    if (v == 0 && bluetoothBms.hasData()) {
        return (uint16_t)bluetoothBms.getPackVoltageMilliVolts();
    }
    return v;
}

uint32_t Telemetry::getBatteryCurrentMilliAmps() const {
    uint32_t a = s_backend_ptr && s_backend_ptr->getBatteryCurrentMilliAmps ? s_backend_ptr->getBatteryCurrentMilliAmps() : 0;
    if (a == 0 && bluetoothBms.hasData()) {
        int32_t bmsMa = bluetoothBms.getPackCurrentMilliAmps();
        return (uint32_t)(bmsMa < 0 ? -bmsMa : bmsMa);
    }
    return a;
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

SignalState Telemetry::getMotorTempState() const {
    return s_backend_ptr && s_backend_ptr->getMotorTempState ? s_backend_ptr->getMotorTempState() : SignalState::Absent;
}

SignalState Telemetry::getEscTempState() const {
    return s_backend_ptr && s_backend_ptr->getEscTempState ? s_backend_ptr->getEscTempState() : SignalState::Absent;
}

SignalState Telemetry::getBatteryVoltageState() const {
    return s_backend_ptr && s_backend_ptr->getBatteryVoltageState ? s_backend_ptr->getBatteryVoltageState() : SignalState::Absent;
}

Telemetry telemetry;
