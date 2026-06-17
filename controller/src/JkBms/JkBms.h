#ifndef JK_BMS_H
#define JK_BMS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "JkBmsParser.h"

// JK BMS BLE: Service 0xFFE0, Characteristic 0xFFE1 (bidirectional — write + notify)
#define JK_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define JK_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

// Holds two full 300-byte JK02 frames so a brief main-loop processing lag
// cannot drop a complete frame before it is parsed.
#define JK_RX_BUFFER_SIZE 600

class BLEClient;
class BLERemoteCharacteristic;

class JkBms {
public:
    JkBms();
    void init();
    void update();
    void setEnabled(bool enabled);
    void setMacAddress(const String& macAddress);

    bool isConnected()  const { return connected_; }
    bool hasData()      const { return data_.valid; }
    bool hasCellData()  const { return hasCellData_; }

    const char* getStateName() const {
        switch (state_) {
            case Idle:       return "idle";
            case Connecting: return "connecting";
            case Subscribed: return "connected";
            default:         return "unknown";
        }
    }

    uint32_t getPackVoltageMilliVolts() const { return data_.packVoltageMilliVolts; }
    int32_t  getPackCurrentMilliAmps()  const { return data_.packCurrentMilliAmps; }
    uint8_t  getSoCPercent()            const { return data_.socPercent; }
    uint8_t  getCellCount()             const { return data_.cellCount; }
    uint8_t  getTempCount()             const { return data_.tempCount; }
    int16_t  getTempCelsius(uint8_t index) const;

    uint16_t getCellVoltageMilliVolts(uint8_t index) const;
    uint16_t getCellMinMilliVolts()   const;
    uint16_t getCellMaxMilliVolts()   const;
    uint16_t getCellDeltaMilliVolts() const;

private:
    enum State { Idle, Connecting, Subscribed };

    BLEClient*               pClient_;
    BLERemoteCharacteristic* pChar_;   // FFE1 — bidirectional (write + notify)
    QueueHandle_t     txQueue_;
    TaskHandle_t      txTaskHandle_;
    QueueHandle_t     connectQueue_;
    TaskHandle_t      connectTaskHandle_;
    SemaphoreHandle_t stateMutex_;
    uint32_t          connectSessionId_;
    State             state_;
    bool              enabled_;
    bool              connected_;
    String            macAddress_;
    unsigned long     lastConnectAttempt_;
    unsigned long     lastRequestMillis_;
    unsigned long     lastCellDataMillis_;
    bool              deviceInfoRequested_;
    bool              hasCellData_;

    static const unsigned long CONNECT_RETRY_MS      = 10000;
    static const unsigned long REQUEST_INTERVAL_MS   = 2000;
    // Once cell-info frames are streaming, the BMS pushes them on its own. Only
    // re-request (which makes the JK beep on each command) if the stream stalls.
    static const unsigned long CELL_STREAM_TIMEOUT_MS = 3000;

    uint8_t   rxBuffer_[JK_RX_BUFFER_SIZE];
    size_t    rxLen_;

    JkProtocol protocol_;
    char       hwVersion_[16];
    JkBmsData  data_;

    void processRxBuffer();
    void sendCommand(uint8_t cmd);
    void printFrameHex(const uint8_t* data, size_t len);
    void resetConnection();
    void applyResetConnectionLocked();
    static void txTask(void* arg);
    static void connectTask(void* arg);

public:
    void onNotify(uint8_t* data, size_t len);
};

#endif // JK_BMS_H
