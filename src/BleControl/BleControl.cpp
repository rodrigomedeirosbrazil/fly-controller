#include "BleControl.h"
#include "../config.h"
#include "../BleServerHost/BleServerHost.h"
#include "../BoardConfig.h"
#include "../Version.h"
#include "../DisarmReason.h"
#include "../Power/Power.h"
#include "../Throttle/Throttle.h"
#include "../Telemetry/SignalState.h"
#include "../Telemetry/MotorTempOrigin.h"
#include "../Telemetry/TelemetryAvailability.h"
#include "../Telemetry/Telemetry.h"
#include "../BatteryMonitor/BatteryMonitor.h"
#include "../BluetoothBms/BluetoothBms.h"

namespace {
// INFO payload. Read once per connection, so it carries only what never
// changes while the controller is up.
#pragma pack(push, 1)
struct ControlInfo {
    uint8_t  protocolVersion;
    uint8_t  controllerType;
    uint16_t capabilities;
    char     appVersion[24];   // NUL-padded
};
#pragma pack(pop)

namespace Capability {
    enum : uint16_t {
        CanTelemetry       = 1 << 0,
        VoltageSensor      = 1 << 1,
        MotorTempSourceSel = 1 << 2,
        RemoteLink         = 1 << 3,
    };
}
} // namespace

void BleControl::init() {
    service_ = bleServerHost.getServer()->createService(BLE_CONTROL_SERVICE_UUID);

    infoChar_ = service_->createCharacteristic(
        BLE_CONTROL_INFO_UUID, BLECharacteristic::PROPERTY_READ);

    telemetryChar_ = service_->createCharacteristic(
        BLE_CONTROL_TELEMETRY_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    telemetryChar_->addDescriptor(new BLE2902());

    cmdChar_ = service_->createCharacteristic(
        BLE_CONTROL_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);

    rspChar_ = service_->createCharacteristic(
        BLE_CONTROL_RSP_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    rspChar_->addDescriptor(new BLE2902());

    writeInfo();
    service_->start();

    Serial.println("BleControl service registered");
}

void BleControl::handle() {
    if (millis() - lastTelemetryMs_ < TELEMETRY_INTERVAL_MS) {
        return;
    }
    lastTelemetryMs_ = millis();
    notifyTelemetry();
}

void BleControl::writeInfo() {
    ControlInfo info = {};
    info.protocolVersion = CONTROL_PROTOCOL_VERSION;
    info.controllerType  = (uint8_t) CONTROLLER_TYPE;

    // VoltageSensor is unconditional: the web portal's hasVoltageSensor is
    // true for IS_XAG and IS_TMOTOR, which is every build that exists.
    uint16_t caps = Capability::RemoteLink | Capability::VoltageSensor;
#if IS_TMOTOR
    caps |= Capability::CanTelemetry | Capability::MotorTempSourceSel;
#endif
    info.capabilities = caps;

    strncpy(info.appVersion, APP_VERSION, sizeof(info.appVersion) - 1);

    infoChar_->setValue((uint8_t*) &info, sizeof(info));
}

void BleControl::fillTelemetry(ControlTelemetry& t) const {
    memset(&t, 0, sizeof(t));
    t.ver = CONTROL_PROTOCOL_VERSION;

    uint8_t flags = 0;
    if (throttle.isArmed())                 flags |= TelemFlag::Armed;
    if (throttle.isEngaged())               flags |= TelemFlag::Engaged;
    if (telemetry.hasData())                flags |= TelemFlag::HasTelemetry;
    if (settings.getPowerControlEnabled())  flags |= TelemFlag::PowerControlEnabled;
    if (bluetoothBms.isConnected())         flags |= TelemFlag::BmsConnected;
    if (settings.getBmsType() != BmsTypeNone && settings.getBmsMac().length() >= 17) {
        flags |= TelemFlag::BmsConfigured;
    }
    t.flags = flags;

    uint16_t valid = 0;
    if (isCurrentAvailable())               valid |= TelemValid::Current;
    if (isRpmAvailable())                   valid |= TelemValid::Rpm;
    if (isPowerKwAvailable())               valid |= TelemValid::PowerKw;
    if (isBmsDataAvailable())               valid |= TelemValid::Bms;
    if (isBmsCellDataAvailable())           valid |= TelemValid::BmsCells;
    t.validity = valid;

    // Motor temp, ESC temp and battery voltage report health through
    // signalStates only -- see the TelemValid comment for why they have no
    // availability bit.

    t.disarmReason = (uint8_t) throttle.getDisarmReason();
    t.signalStates = packSignalStates((uint8_t) telemetry.getMotorTempState(),
                                      (uint8_t) telemetry.getEscTempState(),
                                      (uint8_t) telemetry.getBatteryVoltageState());
    t.motorTempSrc = (uint8_t) telemetry.getMotorTempOrigin();

    t.socCc       = batteryMonitor.getSoC();
    t.socVolt     = batteryMonitor.getSoCFromVoltage();
    t.throttlePct = (uint8_t) throttle.getThrottlePercentage();
    t.powerPct    = (uint8_t) power.getPower();
    t.powerScale  = button.getPowerScale();
    t.armCharge   = button.getArmCharge();
    t.limitCauses = powerAlert.getActiveCauses();

    const uint16_t batteryMv  = telemetry.getBatteryVoltageMilliVolts();
    const uint32_t currentMa  = telemetry.getBatteryCurrentMilliAmps();

    t.batteryMv   = batteryMv;
    t.throttleRaw = (uint16_t) throttle.getThrottleRaw();
    if (isPowerKwAvailable()) {
        t.powerKwX10 = (uint16_t) ((((uint32_t) batteryMv * currentMa) / 1000) / 100000);
    }
    t.escCurrentMa = (int32_t) currentMa;
    t.rpm          = telemetry.getRpm();
    t.motorTempMc  = telemetry.getMotorTempMilliCelsius();
    t.escTempMc    = telemetry.getEscTempMilliCelsius();
    t.sessionSec   = hourMeter.getSessionSec();
    t.hourMeterSec = hourMeter.getHourMeterSec();

    if (isBmsCellDataAvailable()) {
        t.bmsCellMinMv   = bluetoothBms.getCellMinMilliVolts();
        t.bmsCellMaxMv   = bluetoothBms.getCellMaxMilliVolts();
        t.bmsCellDeltaMv = bluetoothBms.getCellDeltaMilliVolts();
    }
    if (isBmsDataAvailable() && bluetoothBms.getTempCount() > 0) {
        int16_t maxTemp = bluetoothBms.getTempCelsius(0);
        for (uint8_t i = 1; i < bluetoothBms.getTempCount(); i++) {
            const int16_t candidate = bluetoothBms.getTempCelsius(i);
            if (candidate > maxTemp) {
                maxTemp = candidate;
            }
        }
        t.bmsTempMaxC = maxTemp;
    }

    t.uptimeSec = (uint32_t) (millis() / 1000UL);
}

void BleControl::notifyTelemetry() {
    ControlTelemetry t;
    fillTelemetry(t);
    telemetryChar_->setValue((uint8_t*) &t, sizeof(t));
    telemetryChar_->notify();
}
