#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

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

    // Current values
    uint16_t batteryCapacityMah;
    uint16_t batteryMinVoltage;
    uint16_t batteryMaxVoltage;
    int32_t motorMaxTemp;
    int32_t motorTempReductionStart;
    int32_t escMaxTemp;
    int32_t escTempReductionStart;
};

#endif // SETTINGS_H

