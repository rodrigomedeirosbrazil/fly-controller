#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../config_controller.h"

enum BmsType : uint8_t {
    BmsTypeNone = 0,
    BmsTypeJbd = 1,
    BmsTypeDaly = 2,
    BmsTypeJk = 3
};

enum ThrottleSource : uint8_t {
    ThrottleSourceWired = 0,
    ThrottleSourceWireless = 1
};

#if IS_TMOTOR
enum MotorTempSource : uint8_t {
    MotorTempSourceCan = 0,
    MotorTempSourceAds1115 = 1
};
#endif

class Settings {
public:
    Settings();
    void init();
    void load();
    void save();

    // Battery capacity
    uint16_t getBatteryCapacityMah() const;
    void setBatteryCapacityMah(uint16_t mAh);

    // Battery voltage
    uint16_t getBatteryMinVoltage() const;
    void setBatteryMinVoltage(uint16_t mv);
    uint16_t getBatteryMaxVoltage() const;
    void setBatteryMaxVoltage(uint16_t mv);

    // Motor temperature
    int32_t getMotorMaxTemp() const;
    void setMotorMaxTemp(int32_t temp);
    int32_t getMotorTempReductionStart() const;
    void setMotorTempReductionStart(int32_t temp);

    // ESC temperature
    int32_t getEscMaxTemp() const;
    void setEscMaxTemp(int32_t temp);
    int32_t getEscTempReductionStart() const;
    void setEscTempReductionStart(int32_t temp);

    // Power control
    bool getPowerControlEnabled() const;
    void setPowerControlEnabled(bool enabled);

    // Bluetooth BMS
    uint8_t getBmsType() const;
    void setBmsType(uint8_t type);
    String getBmsMac() const;
    void setBmsMac(const char* mac);

    // Config PIN — required on all write endpoints and OTA. Default: "0000".
    String getConfigPin() const;
    void setConfigPin(const String& pin);

    // Buzzer volume (0-100%, maps directly to PWM duty cycle)
    uint8_t getBuzzerVolume() const;
    void setBuzzerVolume(uint8_t percent);

    // Voltage divider calibration ratio (XAG/Tmotor: compensates resistor tolerance)
    float getVoltageDividerRatio() const;
    void setVoltageDividerRatio(float ratio);
    float getDefaultVoltageDividerRatio() const;

    // Wireless throttle (ESP-NOW remote). Setters mutate in memory; call save() to persist.
    uint8_t getThrottleSource() const;
    void setThrottleSource(uint8_t source);
    String getRemoteMac() const;          // "" when unpaired
    void setRemoteMac(const char* mac);
    void clearRemoteMac();
#if IS_TMOTOR
    // Motor temperature sensor source (T-Motor only)
    MotorTempSource getMotorTempSource() const;
    void setMotorTempSource(MotorTempSource source);
#endif

private:
    Preferences preferences;
    SemaphoreHandle_t mutex_;

    // Default values
    uint16_t getDefaultBatteryCapacity() const;
    uint16_t getDefaultBatteryMinVoltage() const;
    uint16_t getDefaultBatteryMaxVoltage() const;
    int32_t getDefaultMotorMaxTemp() const;
    int32_t getDefaultMotorTempReductionStart() const;
    int32_t getDefaultEscMaxTemp() const;
    int32_t getDefaultEscTempReductionStart() const;
    bool getDefaultPowerControlEnabled() const;
    uint8_t getDefaultBmsType() const;
    uint8_t getDefaultBuzzerVolume() const;

    // Current values
    String configPin;
    uint16_t batteryCapacityMah;
#if IS_TMOTOR
    MotorTempSource motorTempSource;
#endif
    uint16_t batteryMinVoltage;
    uint16_t batteryMaxVoltage;
    int32_t motorMaxTemp;
    int32_t motorTempReductionStart;
    int32_t escMaxTemp;
    int32_t escTempReductionStart;
    bool powerControlEnabled;
    uint8_t throttleSource;
    String remoteMac;
    String bmsMac;
    uint8_t bmsType;
    uint8_t buzzerVolume;
    float voltageDividerRatio;
};

#endif // SETTINGS_H
