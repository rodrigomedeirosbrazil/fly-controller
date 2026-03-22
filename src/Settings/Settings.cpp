#include "Settings.h"
#include "../config.h"
#include "../BoardConfig.h"

Settings::Settings() {
    batteryCapacityMah = 0;
    batteryMinVoltage = 0;
    batteryMaxVoltage = 0;
    motorMaxTemp = 0;
    motorTempReductionStart = 0;
    escMaxTemp = 0;
    escTempReductionStart = 0;
    powerControlEnabled = true;
    wifiAutoDisableAfterCalibration = true;
    bmsType = BmsTypeNone;
    throttleCurveGamma = 1.0f;
}

void Settings::init() {
    preferences.begin("flyctrl", false);
    load();
}

void Settings::load() {
    // Load battery capacity (default based on controller type)
    batteryCapacityMah = preferences.getUInt("batCap", getDefaultBatteryCapacity());

    // Load battery voltages (defaults from original config.h)
    batteryMinVoltage = preferences.getUInt("batMinV", getDefaultBatteryMinVoltage());
    batteryMaxVoltage = preferences.getUInt("batMaxV", getDefaultBatteryMaxVoltage());

    // Load motor temperatures (defaults from original config.h)
    motorMaxTemp = preferences.getInt("motMaxT", getDefaultMotorMaxTemp());
    motorTempReductionStart = preferences.getInt("motRedT", getDefaultMotorTempReductionStart());

    // Load ESC temperatures (defaults based on controller type)
    escMaxTemp = preferences.getInt("escMaxT", getDefaultEscMaxTemp());
    escTempReductionStart = preferences.getInt("escRedT", getDefaultEscTempReductionStart());

    // Load power control enabled (default: true)
    powerControlEnabled = preferences.getBool("pwrCtrl", getDefaultPowerControlEnabled());

    // Load Wi-Fi auto-disable after throttle calibration (default: enabled)
    wifiAutoDisableAfterCalibration = preferences.getBool("wifiAutoOffCal", getDefaultWifiAutoDisableAfterCalibration());

    // Load generic Bluetooth BMS settings, falling back to legacy JBD keys.
    if (preferences.isKey("bmsType") || preferences.isKey("bmsMac")) {
        bmsType = preferences.getUChar("bmsType", getDefaultBmsType());
        bmsMac = preferences.getString("bmsMac", "");
    } else {
        const String legacyMac = preferences.getString("jbdBmsMac", "");
        const bool legacyEnabled = preferences.getBool("jbdBmsEn", false);
        if (legacyEnabled && legacyMac.length() >= 17) {
            bmsType = BmsTypeJbd;
            bmsMac = legacyMac;
        } else {
            bmsType = getDefaultBmsType();
            bmsMac = "";
        }
    }

    // Load throttle curve gamma (1.0 = linear; higher = less sensitive at low throttle)
    throttleCurveGamma = preferences.getFloat("thrCurveG", getDefaultThrottleCurveGamma());
}

void Settings::save() {
    preferences.putUInt("batCap", batteryCapacityMah);
    preferences.putUInt("batMinV", batteryMinVoltage);
    preferences.putUInt("batMaxV", batteryMaxVoltage);
    preferences.putInt("motMaxT", motorMaxTemp);
    preferences.putInt("motRedT", motorTempReductionStart);
    preferences.putInt("escMaxT", escMaxTemp);
    preferences.putInt("escRedT", escTempReductionStart);
    preferences.putBool("pwrCtrl", powerControlEnabled);
    preferences.putBool("wifiAutoOffCal", wifiAutoDisableAfterCalibration);
    preferences.putUChar("bmsType", bmsType);
    preferences.putString("bmsMac", bmsMac);
    preferences.putFloat("thrCurveG", throttleCurveGamma);
}

uint16_t Settings::getBatteryCapacityMah() const {
    return batteryCapacityMah;
}

void Settings::setBatteryCapacityMah(uint16_t mAh) {
    batteryCapacityMah = mAh;
}

uint16_t Settings::getBatteryMinVoltage() const {
    return batteryMinVoltage;
}

void Settings::setBatteryMinVoltage(uint16_t mv) {
    batteryMinVoltage = mv;
}

uint16_t Settings::getBatteryMaxVoltage() const {
    return batteryMaxVoltage;
}

void Settings::setBatteryMaxVoltage(uint16_t mv) {
    batteryMaxVoltage = mv;
}

int32_t Settings::getMotorMaxTemp() const {
    return motorMaxTemp;
}

void Settings::setMotorMaxTemp(int32_t temp) {
    motorMaxTemp = temp;
}

int32_t Settings::getMotorTempReductionStart() const {
    return motorTempReductionStart;
}

void Settings::setMotorTempReductionStart(int32_t temp) {
    motorTempReductionStart = temp;
}

int32_t Settings::getEscMaxTemp() const {
    return escMaxTemp;
}

void Settings::setEscMaxTemp(int32_t temp) {
    escMaxTemp = temp;
}

int32_t Settings::getEscTempReductionStart() const {
    return escTempReductionStart;
}

void Settings::setEscTempReductionStart(int32_t temp) {
    escTempReductionStart = temp;
}

bool Settings::getPowerControlEnabled() const {
    return powerControlEnabled;
}

void Settings::setPowerControlEnabled(bool enabled) {
    powerControlEnabled = enabled;
}

float Settings::getThrottleCurveGamma() const {
    return throttleCurveGamma;
}

void Settings::setThrottleCurveGamma(float gamma) {
    throttleCurveGamma = constrain(gamma, THROTTLE_CURVE_GAMMA_MIN, THROTTLE_CURVE_GAMMA_MAX);
}

bool Settings::getWifiAutoDisableAfterCalibration() const {
    return wifiAutoDisableAfterCalibration;
}

void Settings::setWifiAutoDisableAfterCalibration(bool enabled) {
    wifiAutoDisableAfterCalibration = enabled;
}

uint16_t Settings::getDefaultBatteryCapacity() const {
    return getBoardConfig().defaultBatteryCapacity;
}

uint16_t Settings::getDefaultBatteryMinVoltage() const {
    return 44100;  // 44.100 V - ~3.15 V per cell (14S)
}

uint16_t Settings::getDefaultBatteryMaxVoltage() const {
    return 58500;  // 58.500 V - 4.15 V per cell (14S)
}

int32_t Settings::getDefaultMotorMaxTemp() const {
    return 100000;  // 100.000°C (note: original comment said 60°C but value is 100000)
}

int32_t Settings::getDefaultMotorTempReductionStart() const {
    return 80000;  // 80.000°C (note: original comment said 50°C but value is 80000)
}

int32_t Settings::getDefaultEscMaxTemp() const {
    return getBoardConfig().defaultEscMaxTemp;
}

int32_t Settings::getDefaultEscTempReductionStart() const {
    return getBoardConfig().defaultEscTempReductionStart;
}

bool Settings::getDefaultPowerControlEnabled() const {
    return true;  // Power control enabled by default
}

bool Settings::getDefaultWifiAutoDisableAfterCalibration() const {
    return true;  // Keep current behavior by default
}

uint8_t Settings::getBmsType() const {
    return bmsType;
}

void Settings::setBmsType(uint8_t type) {
    if (type > BmsTypeDaly) {
        type = BmsTypeNone;
    }
    bmsType = type;
}

String Settings::getBmsMac() const {
    return bmsMac;
}

void Settings::setBmsMac(const char* mac) {
    if (mac != nullptr) {
        bmsMac = String(mac);
    } else {
        bmsMac = "";
    }
}

uint8_t Settings::getDefaultBmsType() const {
    return BmsTypeNone;
}

float Settings::getDefaultThrottleCurveGamma() const {
    return 1.0f;  // Linear response by default
}
