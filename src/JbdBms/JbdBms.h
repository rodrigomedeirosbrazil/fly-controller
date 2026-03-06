#ifndef JBD_BMS_H
#define JBD_BMS_H

#include <Arduino.h>

// JBD BMS uses fixed BLE address (no scan). Define JBD_BMS_BLE_ADDRESS in config.h.

// JBD GATT: Service 0xFF00, RX (Module->Phone) 0xFF01, TX (Phone->Module) 0xFF02
#define JBD_SERVICE_UUID     "0000ff00-0000-1000-8000-00805f9b34fb"
#define JBD_CHAR_UUID_RX     "0000ff01-0000-1000-8000-00805f9b34fb"
#define JBD_CHAR_UUID_TX     "0000ff02-0000-1000-8000-00805f9b34fb"

// Frame: 0xDD payload checksum(2) 0x77; read cmd = 0xA5, reg 0x03 = Basic Info
#define JBD_FRAME_START      0xDD
#define JBD_FRAME_END        0x77
#define JBD_CMD_READ         0xA5
#define JBD_REG_BASIC_INFO   0x03

#define JBD_MAX_NTC          8
#define JBD_RX_BUFFER_SIZE   256

class BLEClient;
class BLECharacteristic;
class BLERemoteCharacteristic;

class JbdBms {
public:
    JbdBms();
    void init();
    void update();

    bool isConnected() const { return connected_; }
    bool hasData() const { return hasData_; }

    uint32_t getPackVoltageMilliVolts() const { return packVoltageMilliVolts_; }
    int32_t  getPackCurrentMilliAmps() const  { return packCurrentMilliAmps_; }
    uint8_t  getSoCPercent() const            { return socPercent_; }
    uint8_t  getCellCount() const             { return cellCount_; }
    uint8_t  getNtcCount() const              { return ntcCount_; }
    int16_t  getNtcTempCelsius(uint8_t index) const;
    uint16_t getCycleCount() const            { return cycleCount_; }
    uint16_t getDesignCapacityMahl() const   { return designCapacityMahl_; }
    uint16_t getBalanceCapacityMahl() const  { return balanceCapacityMahl_; }
    uint8_t  getChgFetEnabled() const        { return chgFetEnabled_; }
    uint8_t  getDsgFetEnabled() const       { return dsgFetEnabled_; }
    uint16_t getCurrentErrors() const        { return currentErrors_; }

private:
    enum State {
        Idle,
        Connecting,
        Connected,
        Subscribed
    };

    BLEClient* pClient_;
    BLERemoteCharacteristic* pCharRx_;
    BLERemoteCharacteristic* pCharTx_;
    State state_;
    bool connected_;
    bool hasData_;
    unsigned long lastConnectAttempt_;
    unsigned long lastRequestMillis_;
    static const unsigned long CONNECT_RETRY_MS = 5000;
    static const unsigned long REQUEST_INTERVAL_MS = 1500;
    static const unsigned long CONNECT_TIMEOUT_MS = 10000;

    uint8_t rxBuffer_[JBD_RX_BUFFER_SIZE];
    size_t rxLen_;

    uint32_t packVoltageMilliVolts_;
    int32_t  packCurrentMilliAmps_;
    uint8_t  socPercent_;
    uint8_t  cellCount_;
    uint8_t  ntcCount_;
    int16_t  ntcTempsCelsius_[JBD_MAX_NTC];
    uint16_t cycleCount_;
    uint16_t designCapacityMahl_;
    uint16_t balanceCapacityMahl_;
    uint8_t  chgFetEnabled_;
    uint8_t  dsgFetEnabled_;
    uint16_t currentErrors_;

    void buildReadFrame(uint8_t reg, uint8_t len, uint8_t* out, size_t* outLen);
    void sendBasicInfoRequest();
    void processRxBuffer();
    void parseBasicInfo(const uint8_t* payload, size_t payloadLen);
    void printFrameHex(const uint8_t* data, size_t len);
    void printBasicInfo();
    void onNotify(uint8_t* data, size_t len);

    friend void onNotifyCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
};

#endif // JBD_BMS_H
