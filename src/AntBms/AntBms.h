#ifndef ANT_BMS_H
#define ANT_BMS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "AntBmsParser.h"


// ANT BMS BLE: Service 0xFFE0, Characteristic 0xFFE1 (write-without-response + notify)
#define ANT_SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define ANT_CHAR_UUID    "0000ffe1-0000-1000-8000-00805f9b34fb"

class BLEClient;
class BLERemoteCharacteristic;

class AntBms {
public:
    AntBms();
    void init();
    void update();
    void setEnabled(bool enabled);
    void setMacAddress(const String& macAddress);

    bool isConnected()  const { return connected_; }
    bool hasData()      const { return data_.valid; }
    bool hasCellData()  const { return data_.valid && data_.cellCount > 0; }

    const char* getStateName() const {
        switch (state_) {
            case Idle:       return "idle";
            case Connecting: return "connecting";
            case Subscribed: return "connected";
            default:         return "unknown";
        }
    }

    uint32_t getPackVoltageMilliVolts() const { return data_.packMv; }
    int32_t  getPackCurrentMilliAmps()  const { return data_.packCurrentMa; }
    uint8_t  getSoCPercent()            const { return data_.socPercent; }
    uint8_t  getCellCount()             const { return data_.cellCount; }
    uint8_t  getTempCount()             const { return data_.probeCount; }
    uint32_t getDesignCapacityMah()     const { return data_.nominalCapacityMah; }
    int16_t  getTempCelsius(uint8_t index) const;

    uint16_t getCellVoltageMilliVolts(uint8_t index) const;
    uint16_t getCellMinMilliVolts()   const { return data_.valid ? data_.cellMinMv : 0; }
    uint16_t getCellMaxMilliVolts()   const { return data_.valid ? data_.cellMaxMv : 0; }
    uint16_t getCellDeltaMilliVolts() const { return data_.valid ? data_.cellDeltaMv : 0; }

    // Called from the BLE stack task.
    void onNotify(uint8_t* data, size_t len);

private:
    enum State { Idle, Connecting, Subscribed };

    BLEClient*               pClient_;
    BLERemoteCharacteristic* pChar_;
    QueueHandle_t     txQueue_;
    TaskHandle_t      txTaskHandle_;
    QueueHandle_t     connectQueue_;
    TaskHandle_t      connectTaskHandle_;
    SemaphoreHandle_t stateMutex_;
    uint32_t          connectSessionId_;
    volatile State    state_;
    bool              initialized_;
    bool              enabled_;
    volatile bool     connected_;
    String            macAddress_;
    unsigned long     lastConnectAttempt_;
    unsigned long     lastRequestMillis_;
    unsigned long     lastStatusMs_;
    bool              statusSeen_;

    static const unsigned long CONNECT_RETRY_MS    = 10000;
    static const unsigned long REQUEST_INTERVAL_MS = 2000;
    static const unsigned long STALE_TIMEOUT_MS    = 10000;

    // Shared with the BLE task, guarded by stateMutex_.
    AntFrameAssembler assembler_;
    uint8_t           pendingFrame_[ANT_MAX_FRAME];
    size_t            pendingLen_;

    // Loop task only.
    AntBmsData data_;

    void processPending();
    void sendStatusRequest();
    void resetConnection();
    void applyResetConnectionLocked();
    static void txTask(void* arg);
    static void connectTask(void* arg);

};

#endif // ANT_BMS_H
