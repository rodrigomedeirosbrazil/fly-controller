#include "Settings.h"
#include "../config.h"
#include "../BoardConfig.h"
#include <cstring>

namespace {

// Max chars for BMS MAC NVS reads (17 + ':' + NUL + small margin). Using Preferences::getString(key, buf, maxLen)
// avoids the Arduino overload that allocates char buf[len] on the stack — corrupted NVS length can overflow the stack and reboot.
constexpr size_t kMaxBmsMacNvsReadChars = 32;

String readBoundedPrefString(Preferences& prefs, const char* key) {
    char buf[kMaxBmsMacNvsReadChars + 1];
    std::memset(buf, 0, sizeof(buf));
    const size_t n = prefs.getString(key, buf, sizeof(buf));
    if (n == 0) {
        return String();
    }
    buf[sizeof(buf) - 1] = '\0';
    return String(buf);
}

bool isValidBmsMacFormat(const String& s) {
    if (s.length() == 0) {
        return true;
    }
    if (s.length() != 17) {
        return false;
    }
    for (int i = 0; i < 17; i++) {
        if (i == 2 || i == 5 || i == 8 || i == 11 || i == 14) {
            if (s.charAt(i) != ':') {
                return false;
            }
        } else {
            const char c = s.charAt(i);
            const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
            if (!hex) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

Settings::Settings() {
    configPin = "0000";
    batteryCapacityMah = 0;
    batteryMinVoltage = 0;
    batteryMaxVoltage = 0;
    motorMaxTemp = 0;
    motorTempReductionStart = 0;
    escMaxTemp = 0;
    escTempReductionStart = 0;
    powerControlEnabled = true;
    bmsType = BmsTypeNone;
    buzzerVolume = getDefaultBuzzerVolume();
    voltageDividerRatio = getDefaultVoltageDividerRatio();
    throttleSource = ThrottleSourceWired;
    remoteMac = "";
    mutex_ = nullptr;
#if IS_TMOTOR
    motorTempSource = MotorTempSourceCan;
#endif
}

void Settings::init() {
    mutex_ = xSemaphoreCreateMutex();
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

    // Load generic Bluetooth BMS settings, falling back to legacy JBD keys.
    if (preferences.isKey("bmsType") || preferences.isKey("bmsMac")) {
        bmsType = preferences.getUChar("bmsType", getDefaultBmsType());
        bmsMac = readBoundedPrefString(preferences, "bmsMac");
    } else {
        const String legacyMac = readBoundedPrefString(preferences, "jbdBmsMac");
        const bool legacyEnabled = preferences.getBool("jbdBmsEn", false);
        if (legacyEnabled && legacyMac.length() >= 17) {
            bmsType = BmsTypeJbd;
            bmsMac = legacyMac;
        } else {
            bmsType = getDefaultBmsType();
            bmsMac = "";
        }
    }

    // Load config PIN (default "0000")
    configPin = readBoundedPrefString(preferences, "cfgPin");
    if (configPin.length() == 0) configPin = "0000";

    // Load buzzer volume (default 85%), clamp to valid range
    buzzerVolume = preferences.getUChar("buzzVol", getDefaultBuzzerVolume());
    if (buzzerVolume > 100) buzzerVolume = getDefaultBuzzerVolume();

    // Load voltage divider calibration ratio
    voltageDividerRatio = preferences.getFloat("vDivR", getDefaultVoltageDividerRatio());
    if (voltageDividerRatio < 1.0f || voltageDividerRatio > 100.0f) voltageDividerRatio = getDefaultVoltageDividerRatio();

    // Wireless throttle source + paired remote MAC
    throttleSource = preferences.getUChar("thrSrc", ThrottleSourceWired);
    if (throttleSource > ThrottleSourceWireless) throttleSource = ThrottleSourceWired;
    remoteMac = preferences.getString("rmtMac", "");
#if IS_TMOTOR
    motorTempSource = (MotorTempSource)preferences.getUChar("motTmpSrc", MotorTempSourceCan);
    if (motorTempSource > MotorTempSourceAds1115) motorTempSource = MotorTempSourceCan;
#endif

    bool repaired = false;

    bmsMac.trim();
    if (!isValidBmsMacFormat(bmsMac)) {
        if (bmsMac.length() > 0) {
            Serial.println("[Settings] Invalid or oversized BMS MAC in NVS, clearing");
            repaired = true;
        }
        bmsMac = "";
    }

    if (bmsType > BmsTypeJk) {
        Serial.println("[Settings] Invalid BMS type in NVS, resetting to None");
        bmsType = BmsTypeNone;
        repaired = true;
    }

    if (bmsType != BmsTypeNone && bmsMac.length() != 17) {
        Serial.println("[Settings] BMS type requires a valid MAC; disabling BMS in NVS");
        bmsType = BmsTypeNone;
        repaired = true;
    }

    if (repaired) {
        save();
    }
}

void Settings::save() {
    // Protect against concurrent calls from WiFi task and main loop.
    // NVS writes are slow (~ms each); holding the mutex ensures the main loop
    // never reads a partially-updated snapshot of member variables while we write.
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    preferences.putString("cfgPin", configPin);
    preferences.putUInt("batCap", batteryCapacityMah);
    preferences.putUInt("batMinV", batteryMinVoltage);
    preferences.putUInt("batMaxV", batteryMaxVoltage);
    preferences.putInt("motMaxT", motorMaxTemp);
    preferences.putInt("motRedT", motorTempReductionStart);
    preferences.putInt("escMaxT", escMaxTemp);
    preferences.putInt("escRedT", escTempReductionStart);
    preferences.putBool("pwrCtrl", powerControlEnabled);
    preferences.putUChar("bmsType", bmsType);
    preferences.putString("bmsMac", bmsMac);
    preferences.putUChar("buzzVol", buzzerVolume);
    preferences.putFloat("vDivR", voltageDividerRatio);
    preferences.putUChar("thrSrc", throttleSource);
    preferences.putString("rmtMac", remoteMac);
#if IS_TMOTOR
    preferences.putUChar("motTmpSrc", (uint8_t)motorTempSource);
#endif
    if (mutex_) xSemaphoreGive(mutex_);
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

uint8_t Settings::getBmsType() const {
    return bmsType;
}

void Settings::setBmsType(uint8_t type) {
    if (type > BmsTypeJk) {
        type = BmsTypeNone;
    }
    bmsType = type;
}

String Settings::getBmsMac() const {
    // String is a heap-allocated object — protect against concurrent access from WiFi task.
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    String copy = bmsMac;
    if (mutex_) xSemaphoreGive(mutex_);
    return copy;
}

void Settings::setBmsMac(const char* mac) {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    if (mac != nullptr) {
        bmsMac = String(mac);
    } else {
        bmsMac = "";
    }
    if (mutex_) xSemaphoreGive(mutex_);
}

uint8_t Settings::getDefaultBmsType() const {
    return BmsTypeNone;
}

String Settings::getConfigPin() const {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    String copy = configPin;
    if (mutex_) xSemaphoreGive(mutex_);
    return copy;
}

void Settings::setConfigPin(const String& pin) {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    configPin = pin;
    if (mutex_) xSemaphoreGive(mutex_);
}

uint8_t Settings::getBuzzerVolume() const {
    return buzzerVolume;
}

void Settings::setBuzzerVolume(uint8_t percent) {
    if (percent > 100) percent = 100;
    buzzerVolume = percent;
}

uint8_t Settings::getThrottleSource() const {
    return throttleSource;
}

void Settings::setThrottleSource(uint8_t source) {
    if (source > ThrottleSourceWireless) {
        source = ThrottleSourceWired;
    }
    throttleSource = source;
}

String Settings::getRemoteMac() const {
    // String is a heap-allocated object — protect against concurrent access from WiFi task.
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    String copy = remoteMac;
    if (mutex_) xSemaphoreGive(mutex_);
    return copy;
}

void Settings::setRemoteMac(const char* mac) {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    remoteMac = (mac != nullptr) ? String(mac) : "";
    if (mutex_) xSemaphoreGive(mutex_);
}

void Settings::clearRemoteMac() {
    if (mutex_) xSemaphoreTake(mutex_, portMAX_DELAY);
    remoteMac = "";
    if (mutex_) xSemaphoreGive(mutex_);
}

uint8_t Settings::getDefaultBuzzerVolume() const {
    return 85;  // Matches the empirically tuned peak-volume duty cycle (217/255)
}

float Settings::getVoltageDividerRatio() const {
    return voltageDividerRatio;
}

void Settings::setVoltageDividerRatio(float ratio) {
    if (ratio < 1.0f || ratio > 100.0f) return;
    voltageDividerRatio = ratio;
}

float Settings::getDefaultVoltageDividerRatio() const {
#if IS_XAG || IS_TMOTOR
    return BATTERY_DIVIDER_RATIO;
#else
    return 1.0f;
#endif
}

#if IS_TMOTOR
MotorTempSource Settings::getMotorTempSource() const {
    return motorTempSource;
}

void Settings::setMotorTempSource(MotorTempSource source) {
    motorTempSource = source;
}
#endif
