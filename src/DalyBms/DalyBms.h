#ifndef DALY_BMS_H
#define DALY_BMS_H

#include <Arduino.h>
#include <stdint.h>

class BLEClient;
class BLERemoteCharacteristic;

class DalyBms {
public:
    DalyBms();

    void init();
    void update();

    void setEnabled(bool enabled);
    void setMacAddress(const String& macAddress);

    bool isConnected() const { return connected_; }
    bool hasData() const { return hasData_; }
    bool hasCellData() const { return hasCellData_; }

    uint32_t getPackVoltageMilliVolts() const { return packVoltageMilliVolts_; }
    int32_t getPackCurrentMilliAmps() const { return packCurrentMilliAmps_; }
    uint8_t getSoCPercent() const { return socPercent_; }
    uint32_t getRemainingCapacityMilliAh() const { return remainingCapacityMilliAh_; }
    uint16_t getCycleCount() const { return cycleCount_; }
    uint8_t getCellCount() const { return cellCount_; }
    uint8_t getTempCount() const { return tempCount_; }
    int16_t getTempCelsius(uint8_t index) const;
    uint16_t getCellVoltageMilliVolts(uint8_t index) const;
    uint16_t getCellMinMilliVolts() const;
    uint16_t getCellMaxMilliVolts() const;
    uint16_t getCellDeltaMilliVolts() const;
    bool isBalanceEnabled() const { return balanceEnabled_; }
    bool isChargeEnabled() const { return chargeEnabled_; }
    bool isDischargeEnabled() const { return dischargeEnabled_; }

    void onNotify(uint8_t* data, size_t len);

private:
    enum State {
        Idle,
        Connecting,
        Connected,
        Subscribed
    };

    static const uint8_t MAX_CELLS = 32;
    static const uint8_t MAX_TEMPS = 8;
    static const size_t RX_BUFFER_SIZE = 192;
    static const unsigned long CONNECT_RETRY_MS = 5000;
    static const unsigned long REQUEST_INTERVAL_MS = 2000;

    BLEClient* pClient_;
    BLERemoteCharacteristic* pCharRx_;
    BLERemoteCharacteristic* pCharTx_;
    State state_;
    bool enabled_;
    bool connected_;
    bool hasData_;
    bool hasCellData_;
    String macAddress_;
    unsigned long lastConnectAttempt_;
    unsigned long lastRequestMillis_;

    uint8_t rxBuffer_[RX_BUFFER_SIZE];
    size_t rxLen_;

    uint32_t packVoltageMilliVolts_;
    int32_t packCurrentMilliAmps_;
    uint8_t socPercent_;
    uint32_t remainingCapacityMilliAh_;
    uint16_t cycleCount_;
    uint8_t cellCount_;
    uint8_t tempCount_;
    int16_t tempsCelsius_[MAX_TEMPS];
    uint16_t cellVoltagesMv_[MAX_CELLS];
    bool balanceEnabled_;
    bool chargeEnabled_;
    bool dischargeEnabled_;

    void resetConnection();
    void sendStatusRequest();
    void processRxBuffer();
    void parseStatusFrame(const uint8_t* frame, size_t frameLen);
    void printFrameHex(const uint8_t* data, size_t len) const;
    void printStatusSummary() const;
    void printCellVoltages() const;
    static uint16_t crc16Modbus(const uint8_t* data, size_t len);
};

#endif // DALY_BMS_H
