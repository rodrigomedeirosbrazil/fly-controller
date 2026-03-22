#ifndef BLUETOOTH_BMS_H
#define BLUETOOTH_BMS_H

#include <stdint.h>

class BluetoothBms {
public:
    void init();
    void update();

    bool isConnected() const;
    bool hasData() const;
    bool hasCellData() const;

    uint32_t getPackVoltageMilliVolts() const;
    int32_t getPackCurrentMilliAmps() const;
    uint8_t getSoCPercent() const;
    uint8_t getCellCount() const;
    uint8_t getTempCount() const;
    int16_t getTempCelsius(uint8_t index) const;

    uint16_t getCellVoltageMilliVolts(uint8_t index) const;
    uint16_t getCellMinMilliVolts() const;
    uint16_t getCellMaxMilliVolts() const;
    uint16_t getCellDeltaMilliVolts() const;

private:
    uint8_t getActiveType() const;
};

#endif // BLUETOOTH_BMS_H
