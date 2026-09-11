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
#include "../Settings/SettingsValidation.h"
#include <math.h>

namespace {

class CmdCallbacks : public BLECharacteristicCallbacks {
public:
    explicit CmdCallbacks(BleControl* owner) : owner_(owner) {}

    void onWrite(BLECharacteristic* characteristic) override {
        const std::string value = characteristic->getValue();
        owner_->enqueueFromCallback((const uint8_t*) value.data(), value.size());
    }

private:
    BleControl* owner_;
};

// "AA:BB:CC:DD:EE:FF" -> 6 raw bytes. All-zero output means unset, matching
// what "" means in Settings.
void macStringToBytes(const String& mac, uint8_t out[6]) {
    memset(out, 0, 6);
    if (mac.length() != 17) {
        return;
    }
    for (uint8_t i = 0; i < 6; i++) {
        out[i] = (uint8_t) strtoul(mac.substring(i * 3, i * 3 + 2).c_str(), nullptr, 16);
    }
}

void macBytesToString(const uint8_t mac[6], char out[18]) {
    bool allZero = true;
    for (uint8_t i = 0; i < 6; i++) {
        if (mac[i] != 0) { allZero = false; break; }
    }
    if (allZero) {
        out[0] = '\0';
        return;
    }
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

} // namespace

void BleControl::init() {
    BLEServer* server = bleServerHost.getServer();
    if (server == nullptr) {
        Serial.println("[BleControl] WARNING: no BLE server -- control service not registered");
        return;
    }

    service_ = server->createService(BLE_CONTROL_SERVICE_UUID);

    infoChar_ = service_->createCharacteristic(
        BLE_CONTROL_INFO_UUID, BLECharacteristic::PROPERTY_READ);

    telemetryChar_ = service_->createCharacteristic(
        BLE_CONTROL_TELEMETRY_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    telemetryChar_->addDescriptor(new BLE2902());

    cmdChar_ = service_->createCharacteristic(
        BLE_CONTROL_CMD_UUID, BLECharacteristic::PROPERTY_WRITE);
    cmdChar_->setCallbacks(new CmdCallbacks(this));

    rspChar_ = service_->createCharacteristic(
        BLE_CONTROL_RSP_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    rspChar_->addDescriptor(new BLE2902());

    writeInfo();
    service_->start();

    Serial.println("BleControl service registered");
}

void BleControl::enqueueFromCallback(const uint8_t* data, size_t len) {
    ControlRequest req;
    if (!decodeRequest(data, len, req)) {
        return;  // malformed frame: nothing to reply to, seq is unknown
    }
    queue_.push(req);  // drop-newest on overflow; the app retries on timeout
}

void BleControl::handle() {
    drainQueue();

    if (millis() - lastTelemetryMs_ < TELEMETRY_INTERVAL_MS) {
        return;
    }
    lastTelemetryMs_ = millis();
    notifyTelemetry();
}

void BleControl::drainQueue() {
    QueuedRequest req;
    while (queue_.pop(req)) {
        dispatch(req);
    }
}

void BleControl::respond(uint8_t op, uint8_t seq, ControlStatus status,
                         const uint8_t* payload, uint8_t len) {
    uint8_t frame[CONTROL_MAX_FRAME];
    const size_t n = encodeResponse(frame, sizeof(frame), op, seq, status, payload, len);
    if (n == 0) {
        return;
    }
    rspChar_->setValue(frame, n);
    rspChar_->notify();
}

void BleControl::dispatch(const QueuedRequest& req) {
    const ControlStatus gate = gateRequest(req.op, authenticated_, throttle.isArmed());
    if (gate != ControlStatus::Ok) {
        respond(req.op, req.seq, gate, nullptr, 0);
        return;
    }

    uint8_t out[CONTROL_MAX_PAYLOAD];
    uint8_t outLen = 0;
    ControlStatus status = ControlStatus::ErrBadOp;

    switch (req.op) {
        case ControlOp::Auth:   status = handleAuth(req); break;
        case ControlOp::CfgGet: status = handleCfgGet(req, out, outLen); break;
        case ControlOp::CfgSet: status = handleCfgSet(req); break;
        default: break;   // actions land here in Task 10
    }

    respond(req.op, req.seq, status, outLen > 0 ? out : nullptr, outLen);
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
        // uint64_t is load-bearing. A 32-bit product overflows above ~73 A at
        // 58.5 V, and TmotorCan reports up to 500 A: at a real 50 V / 150 A
        // the wrapped value reads 3.2 kW instead of 7.5 kW -- wrong exactly
        // at full throttle, where the number matters most.
        t.powerKwX10 = (uint16_t) ((((uint64_t) batteryMv * currentMa) / 1000) / 100000);
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

ControlStatus BleControl::handleAuth(const QueuedRequest& req) {
    // Payload is the PIN as plain characters, not NUL-terminated. Reuses the
    // PIN in Settings rather than BLE bonding: one source of truth, and no
    // OS-level pairing flow for the pilot to manage outside the app.
    const String expected = settings.getConfigPin();
    if (req.len == 0 || req.len != expected.length()) {
        authenticated_ = false;
        return ControlStatus::ErrAuth;
    }
    if (memcmp(req.payload, expected.c_str(), req.len) != 0) {
        authenticated_ = false;
        return ControlStatus::ErrAuth;
    }
    authenticated_ = true;
    return ControlStatus::Ok;
}

ControlStatus BleControl::handleCfgGet(const QueuedRequest& req, uint8_t* out, uint8_t& outLen) {
    ControlRequest asRequest = { req.op, req.seq, req.len, req.payload };
    ConfigGroup group;
    if (!decodeConfigGroup(asRequest, group)) {
        return ControlStatus::ErrBadArg;
    }

    switch (group) {
        case ConfigGroup::Power: {
            ConfigPower cfg;
            cfg.batteryCapacityMah      = settings.getBatteryCapacityMah();
            cfg.batteryMinVoltageMv     = settings.getBatteryMinVoltage();
            cfg.batteryMaxVoltageMv     = settings.getBatteryMaxVoltage();
            cfg.powerControlEnabled     = settings.getPowerControlEnabled() ? 1 : 0;
            cfg.voltageDividerRatioX100 = (uint16_t) lroundf(settings.getVoltageDividerRatio() * 100.0f);
            memcpy(out, &cfg, sizeof(cfg));
            outLen = sizeof(cfg);
            return ControlStatus::Ok;
        }
        case ConfigGroup::Thermal: {
            ConfigThermal cfg;
            cfg.motorTempReductionStartMc = settings.getMotorTempReductionStart();
            cfg.motorMaxTempMc            = settings.getMotorMaxTemp();
            cfg.escTempReductionStartMc   = settings.getEscTempReductionStart();
            cfg.escMaxTempMc              = settings.getEscMaxTemp();
#if IS_TMOTOR
            cfg.motorTempSource = (uint8_t) settings.getMotorTempSource();
#else
            cfg.motorTempSource = 0;
#endif
            memcpy(out, &cfg, sizeof(cfg));
            outLen = sizeof(cfg);
            return ControlStatus::Ok;
        }
        case ConfigGroup::Bms: {
            ConfigBms cfg;
            cfg.bmsType = settings.getBmsType();
            macStringToBytes(settings.getBmsMac(), cfg.bmsMac);
            memcpy(out, &cfg, sizeof(cfg));
            outLen = sizeof(cfg);
            return ControlStatus::Ok;
        }
        case ConfigGroup::System: {
            ConfigSystem cfg;
            cfg.buzzerVolume   = settings.getBuzzerVolume();
            cfg.throttleSource = settings.getThrottleSource();
            macStringToBytes(settings.getRemoteMac(), cfg.remoteMac);
            memcpy(out, &cfg, sizeof(cfg));
            outLen = sizeof(cfg);
            return ControlStatus::Ok;
        }
    }
    return ControlStatus::ErrBadArg;
}

ControlStatus BleControl::handleCfgSet(const QueuedRequest& req) {
    ControlRequest asRequest = { req.op, req.seq, req.len, req.payload };
    ConfigGroup group;
    if (!decodeConfigGroup(asRequest, group)) {
        return ControlStatus::ErrBadArg;
    }

    // Byte 0 is the group; the struct prefix follows it.
    const uint8_t* body    = req.payload + 1;
    const size_t   bodyLen = req.len - 1;

    switch (group) {
        case ConfigGroup::Power: {
            // Seed with the current values so a short write from an older app
            // updates what it knows and leaves the rest alone.
            ConfigPower cfg;
            uint8_t seed[CONTROL_MAX_PAYLOAD];
            uint8_t seedLen = 0;
            QueuedRequest getReq = req;
            getReq.len = 1;
            handleCfgGet(getReq, seed, seedLen);
            memcpy(&cfg, seed, sizeof(cfg));

            copyKnownPrefix(&cfg, sizeof(cfg), body, bodyLen);

            const SettingsError err = validatePower(cfg.batteryCapacityMah,
                                                    cfg.batteryMinVoltageMv,
                                                    cfg.batteryMaxVoltageMv);
            if (err != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }
            const float ratio = cfg.voltageDividerRatioX100 / 100.0f;
            if (validateVoltageDividerRatio(ratio) != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }

            settings.setBatteryCapacityMah(cfg.batteryCapacityMah);
            settings.setBatteryMinVoltage(cfg.batteryMinVoltageMv);
            settings.setBatteryMaxVoltage(cfg.batteryMaxVoltageMv);
            settings.setPowerControlEnabled(cfg.powerControlEnabled != 0);
            settings.setVoltageDividerRatio(ratio);
            settings.save();

            // Same in-memory sync the web handler does: without it Coulomb
            // counting uses the old capacity until reboot.
            batteryMonitor.setCapacity(settings.getBatteryCapacityMah());
#if IS_XAG || IS_TMOTOR
            batterySensor.setDividerRatio(settings.getVoltageDividerRatio());
#endif
            return ControlStatus::Ok;
        }
        case ConfigGroup::Thermal: {
            ConfigThermal cfg;
            uint8_t seed[CONTROL_MAX_PAYLOAD];
            uint8_t seedLen = 0;
            QueuedRequest getReq = req;
            getReq.len = 1;
            handleCfgGet(getReq, seed, seedLen);
            memcpy(&cfg, seed, sizeof(cfg));

            copyKnownPrefix(&cfg, sizeof(cfg), body, bodyLen);

            if (validateThermal(cfg.motorTempReductionStartMc, cfg.motorMaxTempMc,
                                cfg.escTempReductionStartMc,   cfg.escMaxTempMc)
                != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }
            settings.setMotorTempReductionStart(cfg.motorTempReductionStartMc);
            settings.setMotorMaxTemp(cfg.motorMaxTempMc);
            settings.setEscTempReductionStart(cfg.escTempReductionStartMc);
            settings.setEscMaxTemp(cfg.escMaxTempMc);
#if IS_TMOTOR
            // The armed gate already refused this request if armed, which is
            // the same protection the web handler applies to this one field.
            if (validateMotorTempSource(cfg.motorTempSource) != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }
            settings.setMotorTempSource((MotorTempSource) cfg.motorTempSource);
#endif
            settings.save();
            return ControlStatus::Ok;
        }
        case ConfigGroup::Bms: {
            ConfigBms cfg;
            uint8_t seed[CONTROL_MAX_PAYLOAD];
            uint8_t seedLen = 0;
            QueuedRequest getReq = req;
            getReq.len = 1;
            handleCfgGet(getReq, seed, seedLen);
            memcpy(&cfg, seed, sizeof(cfg));

            copyKnownPrefix(&cfg, sizeof(cfg), body, bodyLen);

            if (validateBmsType(cfg.bmsType) != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }
            char macText[18];
            macBytesToString(cfg.bmsMac, macText);
            // Same rule as the web handler: a configured type needs a MAC.
            if (cfg.bmsType != BmsTypeNone && macText[0] == '\0') {
                return ControlStatus::ErrBadArg;
            }
            settings.setBmsType(cfg.bmsType);
            settings.setBmsMac(macText);
            settings.save();
            return ControlStatus::Ok;
        }
        case ConfigGroup::System: {
            ConfigSystem cfg;
            uint8_t seed[CONTROL_MAX_PAYLOAD];
            uint8_t seedLen = 0;
            QueuedRequest getReq = req;
            getReq.len = 1;
            handleCfgGet(getReq, seed, seedLen);
            memcpy(&cfg, seed, sizeof(cfg));

            copyKnownPrefix(&cfg, sizeof(cfg), body, bodyLen);

            if (validateSystem(cfg.buzzerVolume, cfg.throttleSource) != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }
            settings.setBuzzerVolume(cfg.buzzerVolume);
            settings.setThrottleSource(cfg.throttleSource);
            settings.save();
            return ControlStatus::Ok;
        }
    }
    return ControlStatus::ErrBadArg;
}
