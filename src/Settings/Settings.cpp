#include "Settings.h"
#include "../config.h"

Settings::Settings() {
    batteryCapacityMah = 0;
    batteryMinVoltage = 0;
    batteryMaxVoltage = 0;
    motorMaxTemp = 0;
    motorTempReductionStart = 0;
    escMaxTemp = 0;
    escTempReductionStart = 0;
    powerControlEnabled = true;
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

uint16_t Settings::getDefaultBatteryCapacity() const {
    #if IS_HOBBYWING
    return 65000;  // 65.0 Ah for Hobbywing
    #else
    return 18000;  // 18.0 Ah for Tmotor and XAG
    #endif
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
    #if IS_XAG
    return 80000;  // 80.000°C for XAG
    #else
    return 110000;  // 110.000°C for Hobbywing/Tmotor
    #endif
}

int32_t Settings::getDefaultEscTempReductionStart() const {
    #if IS_XAG
    return 70000;  // 70.000°C for XAG
    #else
    return 80000;  // 80.000°C for Hobbywing/Tmotor
    #endif
}

bool Settings::getDefaultPowerControlEnabled() const {
    return true;  // Power control enabled by default
}

