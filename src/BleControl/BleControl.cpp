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
#include <ElegantOTA.h>
#include <sys/time.h>
#include "../Sound/Sound.h"
#include "../RemoteLink/RemoteLink.h"
#if IS_TMOTOR
#include "../Tmotor/TmotorCan.h"
#include "../Canbus/Canbus.h"
#endif
#include <math.h>

namespace {

class CmdCallbacks : public BLECharacteristicCallbacks {
public:
    explicit CmdCallbacks(BleControl* owner) : owner_(owner) {}

    // The param overload, because conn_id is only on the event -- it is not
    // in the frame, and the library calls both overloads.
    void onWrite(BLECharacteristic* characteristic,
                 esp_ble_gatts_cb_param_t* param) override {
        const std::string value = characteristic->getValue();
        const uint16_t connId = (param != nullptr) ? param->write.conn_id
                                                   : CONTROL_NO_CONN_ID;
        owner_->enqueueFromCallback((const uint8_t*) value.data(), value.size(), connId);
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

void BleControl::enqueueFromCallback(const uint8_t* data, size_t len, uint16_t connId) {
    ControlRequest req;
    if (!decodeRequest(data, len, req)) {
        return;  // malformed frame: nothing to reply to, seq is unknown
    }
    queue_.push(req, connId);  // drop-newest on overflow; the app retries on timeout
}

void BleControl::handle() {
    drainQueue();
    notifyNewBeeps();

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

void BleControl::notifyNewBeeps() {
    BeepEvent ring[Sound::kRingSize];
    const uint8_t count = sound.getBeepEvents(ring, Sound::kRingSize);

    for (uint8_t i = 0; i < count; i++) {
        if (!beepEventIsNew(ring[i].seq, beepWatermark_)) {
            continue;
        }
        ControlBeepEvent event;
        event.seq       = ring[i].seq;
        event.frequency = ring[i].frequency;
        event.onMs      = ring[i].onMs;
        event.offMs     = ring[i].offMs;
        event.reps      = ring[i].reps;
        event.layer     = ring[i].layer;
        event.active    = ring[i].active ? 1 : 0;

        respond(ControlOp::EvtBeep, CONTROL_EVENT_SEQ, ControlStatus::Ok,
                (const uint8_t*) &event, sizeof(event));
    }
}

void BleControl::dispatch(const QueuedRequest& req) {
    // Authenticated only if THIS central is the one that authenticated.
    const bool authenticated = (req.connId != CONTROL_NO_CONN_ID) &&
                               (req.connId == authConnId_);
    const ControlStatus gate = gateRequest(req.op, authenticated, throttle.isArmed());
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
        default: status = handleAction(req, out, outLen); break;
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
    const bool ok = req.len > 0 &&
                    req.len == expected.length() &&
                    memcmp(req.payload, expected.c_str(), req.len) == 0;

    if (ok) {
        authConnId_ = req.connId;
        return ControlStatus::Ok;
    }

    // Fail closed, but only for the connection that got it wrong. Clearing
    // authConnId_ unconditionally would let any other central -- or anything
    // that can connect and write -- knock an authenticated session out by
    // sending one bad PIN.
    if (req.connId == authConnId_) {
        authConnId_ = CONTROL_NO_CONN_ID;
    }
    return ControlStatus::ErrAuth;
}

// Fills `dst` with the group's current values so CFG_SET can overlay only the
// prefix the app actually sent. Returns false rather than seeding from an
// uninitialised buffer -- the four call sites would otherwise depend on an
// invariant spread across two functions with nothing stating it.
bool BleControl::seedCurrentConfig(const QueuedRequest& req, void* dst, size_t dstSize) {
    uint8_t seed[CONTROL_MAX_PAYLOAD];
    uint8_t seedLen = 0;

    QueuedRequest getReq = req;
    getReq.len = 1;   // the group byte only; the struct that follows is ignored
    if (handleCfgGet(getReq, seed, seedLen) != ControlStatus::Ok || seedLen < dstSize) {
        return false;
    }
    memcpy(dst, seed, dstSize);
    return true;
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
            ConfigPower cfg;
            if (!seedCurrentConfig(req, &cfg, sizeof(cfg))) {
                return ControlStatus::ErrBadArg;
            }
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
            if (!seedCurrentConfig(req, &cfg, sizeof(cfg))) {
                return ControlStatus::ErrBadArg;
            }
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
            if (!seedCurrentConfig(req, &cfg, sizeof(cfg))) {
                return ControlStatus::ErrBadArg;
            }
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
            if (!seedCurrentConfig(req, &cfg, sizeof(cfg))) {
                return ControlStatus::ErrBadArg;
            }
            copyKnownPrefix(&cfg, sizeof(cfg), body, bodyLen);

            if (validateSystem(cfg.buzzerVolume, cfg.throttleSource) != SettingsError::None) {
                return ControlStatus::ErrBadArg;
            }
            settings.setBuzzerVolume(cfg.buzzerVolume);
            settings.setThrottleSource(cfg.throttleSource);
            settings.save();
            // Same in-memory sync the web handler does. Without it the new
            // volume is persisted but inaudible until the next reboot.
            buzzer.setVolume(cfg.buzzerVolume);
            return ControlStatus::Ok;
        }
    }
    return ControlStatus::ErrBadArg;
}

ControlStatus BleControl::handleAction(const QueuedRequest& req, uint8_t* out, uint8_t& outLen) {
    switch (req.op) {
        case ControlOp::SessionReset:
            // HourMeter applies this on its next tick; the counter is RAM-only.
            hourMeter.requestReset();
            return ControlStatus::Ok;

        case ControlOp::BmsScanStart:
            return bluetoothBms.startWebScan() ? ControlStatus::Ok : ControlStatus::ErrBusy;

        case ControlOp::BmsScanStatus: {
            // [status u8][resultCount u8] then, per result, [mac 6][rssi i8][type u8].
            const uint8_t count = bluetoothBms.getWebScanResultCount();
            const BluetoothBmsScanResult* results = bluetoothBms.getWebScanResults();
            out[0] = bluetoothBms.getWebScanStatus();
            out[1] = count;
            outLen = 2;
            for (uint8_t i = 0; i < count; i++) {
                if ((size_t) outLen + 8 > CONTROL_MAX_PAYLOAD) {
                    break;  // truncate rather than overflow; count says how many existed
                }
                macStringToBytes(results[i].mac, out + outLen);
                outLen += 6;
                out[outLen++] = (uint8_t) (int8_t) results[i].rssi;
                out[outLen++] = results[i].detectedType;
            }

            // On failure, append the reason as a NUL-terminated string after
            // the result list. Without it a client can say a scan failed and
            // never why, which is what made the async-disconnect bug a
            // guessing game from the app side. Appended, so a client that
            // stops reading after `count` results is unaffected.
            if (out[0] == BluetoothBmsScanError) {
                const char* reason = bluetoothBms.getWebScanError();
                const size_t room = CONTROL_MAX_PAYLOAD - outLen;
                if (reason != nullptr && reason[0] != '\0' && room > 1) {
                    const size_t n = strnlen(reason, room - 1);
                    memcpy(out + outLen, reason, n);
                    outLen += (uint8_t) n;
                    out[outLen++] = '\0';
                }
            }
            return ControlStatus::Ok;
        }

        case ControlOp::BmsDetect: {
            if (req.len < 6) {
                return ControlStatus::ErrBadArg;
            }
            char macText[18];
            macBytesToString(req.payload, macText);
            out[0] = bluetoothBms.detectBmsTypeByMac(String(macText));
            outLen = 1;
            return ControlStatus::Ok;
        }

        case ControlOp::RemotePair:
            remoteLink.enterPairing();
            return ControlStatus::Ok;

        case ControlOp::RemoteForget:
            settings.clearRemoteMac();
            settings.save();
            // Clearing NVS alone leaves the running link paired until reboot.
            remoteLink.forgetPeer();
            return ControlStatus::Ok;

        case ControlOp::BuzzerPreview: {
            if (req.len < 1 || req.payload[0] > 100) {
                return ControlStatus::ErrBadArg;
            }
            buzzer.setVolume(req.payload[0]);
            sound.play(SoundEvent::VolumePreview);
            return ControlStatus::Ok;
        }

        case ControlOp::SetTime: {
            // Payload is epoch milliseconds as i64 little-endian. The web
            // route parses the same value out of a text body.
            if (req.len < 8) {
                return ControlStatus::ErrBadArg;
            }
            int64_t epochMs = 0;
            memcpy(&epochMs, req.payload, sizeof(epochMs));
            if (epochMs <= 1577836800000LL) {  // sanity: must be after 2020-01-01
                return ControlStatus::ErrBadArg;
            }
            struct timeval tv;
            tv.tv_sec  = (time_t) (epochMs / 1000);
            tv.tv_usec = (suseconds_t) ((epochMs % 1000) * 1000);
            settimeofday(&tv, nullptr);
            return ControlStatus::Ok;
        }

        case ControlOp::PinChange: {
            // [currentLen u8][current...][newLen u8][new...]
            if (req.len < 2) {
                return ControlStatus::ErrBadArg;
            }
            const uint8_t currentLen = req.payload[0];
            if ((size_t) 1 + currentLen + 1 > req.len) {
                return ControlStatus::ErrBadArg;
            }
            const uint8_t newLen = req.payload[1 + currentLen];
            if ((size_t) 1 + currentLen + 1 + newLen > req.len) {
                return ControlStatus::ErrBadArg;
            }
            if (newLen < 4 || newLen > 8) {
                return ControlStatus::ErrBadArg;
            }

            const String expected = settings.getConfigPin();
            if (currentLen != expected.length() ||
                memcmp(req.payload + 1, expected.c_str(), currentLen) != 0) {
                return ControlStatus::ErrAuth;
            }

            char newPin[9] = {0};
            memcpy(newPin, req.payload + 1 + currentLen + 1, newLen);
            settings.setConfigPin(String(newPin));
            settings.save();
            // Keep the OTA portal's basic auth in step, as the web route does.
            ElegantOTA.setAuth("admin", settings.getConfigPin().c_str());
            return ControlStatus::Ok;
        }

#if IS_TMOTOR
        case ControlOp::TmotorDirForward:
        case ControlOp::TmotorDirReverse: {
            if (canbus.getEscNodeId() == 0) {
                return ControlStatus::ErrState;   // ESC not on the bus yet
            }
            tmotorCan.sendDirectionSet(req.op == ControlOp::TmotorDirForward);
            return ControlStatus::Ok;
        }
#endif

        default:
            return ControlStatus::ErrBadOp;
    }
}
