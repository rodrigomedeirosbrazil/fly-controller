#ifndef JBD_BMS_H
#define JBD_BMS_H

#include <Arduino.h>

// JBD BMS uses fixed BLE address (no scan). Define JBD_BMS_BLE_ADDRESS in config.h.

// JBD GATT: Service 0xFF00
// FF01 = Module->Phone: Notify, Read → pCharRx_ (register notify here)
// FF02 = Phone->Module: Write w/o Rsp → pCharTx_ (write commands here)
#define JBD_SERVICE_UUID     "0000ff00-0000-1000-8000-00805f9b34fb"
#define JBD_CHAR_UUID_RX     "0000ff01-0000-1000-8000-00805f9b34fb"  // notify
#define JBD_CHAR_UUID_TX     "0000ff02-0000-1000-8000-00805f9b34fb"  // write

// Frame: DD | REG | STATUS | LEN | DATA... | CHK_H | CHK_L | 77
// Read request: DD | A5 | REG | 00 | CHK_H | CHK_L | 77
#define JBD_FRAME_START      0xDD
#define JBD_FRAME_END        0x77
#define JBD_CMD_READ         0xA5
#define JBD_CMD_WRITE        0x5A
#define JBD_REG_LOGIN        0x00
#define JBD_REG_BASIC_INFO   0x03
#define JBD_REG_CELL_VOLTAGES 0x04

#define JBD_MAX_NTC          8
#define JBD_MAX_CELLS        32
#define JBD_RX_BUFFER_SIZE   256

class BLEClient;
class BLERemoteCharacteristic;

class JbdBms {
public:
    JbdBms();
    void init();
    void update();

    bool isConnected() const { return connected_; }
    bool hasData()     const { return hasData_; }
    bool hasCellData() const { return hasCellData_; }

    uint32_t getPackVoltageMilliVolts() const { return packVoltageMilliVolts_; }
    int32_t  getPackCurrentMilliAmps()  const { return packCurrentMilliAmps_; }
    uint8_t  getSoCPercent()            const { return socPercent_; }
    uint8_t  getCellCount()             const { return cellCount_; }
    uint8_t  getNtcCount()              const { return ntcCount_; }
    int16_t  getNtcTempCelsius(uint8_t index) const;
    uint16_t getCycleCount()            const { return cycleCount_; }
    uint32_t getDesignCapacityMahl()    const { return designCapacityMahl_; }
    uint32_t getBalanceCapacityMahl()   const { return balanceCapacityMahl_; }
    uint8_t  getChgFetEnabled()         const { return chgFetEnabled_; }
    uint8_t  getDsgFetEnabled()         const { return dsgFetEnabled_; }
    uint16_t getCurrentErrors()         const { return currentErrors_; }

    // Individual cell voltages (mV)
    uint16_t getCellVoltageMilliVolts(uint8_t index) const;
    uint16_t getCellMinMilliVolts()  const;
    uint16_t getCellMaxMilliVolts()  const;
    uint16_t getCellDeltaMilliVolts() const; // max-min difference (balance indicator)

private:
    enum State {
        Idle,
        Connecting,
        Connected,
        Subscribed
    };

    BLEClient*               pClient_;
    BLERemoteCharacteristic* pCharRx_;  // FF01 - BMS->ESP (notify)
    BLERemoteCharacteristic* pCharTx_;  // FF02 - ESP->BMS (write)
    State        state_;
    bool         connected_;
    bool         hasData_;
    unsigned long lastConnectAttempt_;
    unsigned long lastRequestMillis_;

    static const unsigned long CONNECT_RETRY_MS   = 5000;
    static const unsigned long REQUEST_INTERVAL_MS = 2000;

    uint8_t rxBuffer_[JBD_RX_BUFFER_SIZE];
    size_t  rxLen_;

    // Decoded data — register 0x03
    uint32_t packVoltageMilliVolts_;
    int32_t  packCurrentMilliAmps_;
    uint8_t  socPercent_;
    uint8_t  cellCount_;
    uint8_t  ntcCount_;
    int16_t  ntcTempsCelsius_[JBD_MAX_NTC];
    uint16_t cycleCount_;
    uint32_t designCapacityMahl_;
    uint32_t balanceCapacityMahl_;
    uint8_t  chgFetEnabled_;
    uint8_t  dsgFetEnabled_;
    uint16_t currentErrors_;

    // Decoded data — register 0x04
    bool     hasCellData_;
    uint16_t cellVoltagesMv_[JBD_MAX_CELLS];

    // Alternating request control 0x03 / 0x04
    bool     requestCells_; // alternate between requesting 0x03 and 0x04

    // Internos
    void buildReadFrame(uint8_t reg, uint8_t* out, size_t* outLen);
    void buildWriteFrame(uint8_t reg, const uint8_t* data, uint8_t dataLen, uint8_t* out, size_t* outLen);
    void sendLoginRequest();
    void sendBasicInfoRequest();
    void sendCellVoltageRequest();
    void processRxBuffer();
    void parseBasicInfo(const uint8_t* data, size_t dataLen);
    void parseCellVoltages(const uint8_t* data, size_t dataLen);
    void printFrameHex(const uint8_t* data, size_t len);
    void printBasicInfo();
    void printCellVoltages();
    void resetConnection();

public:
    // Called by global notify callback — do not call directly
    void onNotify(uint8_t* data, size_t len);
};

#endif // JBD_BMS_H
