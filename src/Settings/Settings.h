#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

enum BmsType : uint8_t {
    BmsTypeNone = 0,
    BmsTypeJbd = 1,
    BmsTypeDaly = 2
};

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

    // Throttle curve (power law: y = x^gamma; 1.0 = linear)
    float getThrottleCurveGamma() const;
    void setThrottleCurveGamma(float gamma);

    // Wi-Fi behavior
    bool getWifiAutoDisableAfterCalibration() const;
    void setWifiAutoDisableAfterCalibration(bool enabled);

    // Bluetooth BMS
    uint8_t getBmsType() const;
    void setBmsType(uint8_t type);
    String getBmsMac() const;
    void setBmsMac(const char* mac);

private:
    Preferences preferences;

    // Default values
    uint16_t getDefaultBatteryCapacity() const;
    uint16_t getDefaultBatteryMinVoltage() const;
    uint16_t getDefaultBatteryMaxVoltage() const;
    int32_t getDefaultMotorMaxTemp() const;
    int32_t getDefaultMotorTempReductionStart() const;
    int32_t getDefaultEscMaxTemp() const;
    int32_t getDefaultEscTempReductionStart() const;
    bool getDefaultPowerControlEnabled() const;
    bool getDefaultWifiAutoDisableAfterCalibration() const;
    uint8_t getDefaultBmsType() const;
    float getDefaultThrottleCurveGamma() const;

    // Current values
    uint16_t batteryCapacityMah;
    uint16_t batteryMinVoltage;
    uint16_t batteryMaxVoltage;
    int32_t motorMaxTemp;
    int32_t motorTempReductionStart;
    int32_t escMaxTemp;
    int32_t escTempReductionStart;
    bool powerControlEnabled;
    bool wifiAutoDisableAfterCalibration;
    String bmsMac;
    uint8_t bmsType;
    float throttleCurveGamma;
};

#endif // SETTINGS_H
