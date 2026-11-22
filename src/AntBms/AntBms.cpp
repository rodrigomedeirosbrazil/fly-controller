#include "AntBms.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include <cstring>
#include <BLEDevice.h>
#include <BLEClient.h>
#include "../BluetoothBms/BmsLinkParams.h"

struct AntTxFrame {
    uint8_t data[ANT_REQUEST_SIZE];
    size_t  len;
};

static AntBms* s_antBms = nullptr;

// ---------------------------------------------------------------------------
// Global notify callback
// ---------------------------------------------------------------------------
void onAntNotifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (s_antBms && pData && length > 0) {
        s_antBms->onNotify(pData, length);
    }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
AntBms::AntBms()
    : pClient_(nullptr), pChar_(nullptr),
      txQueue_(nullptr), connectQueue_(nullptr), stateMutex_(nullptr),
      connectSessionId_(0),
      state_(Idle), initialized_(false), enabled_(false), connected_(false),
      lastConnectAttempt_(0), lastRequestMillis_(0), lastStatusMs_(0),
      connectTaskStarted_(false), pendingLen_(0)
{
    memset(pendingFrame_, 0, sizeof(pendingFrame_));
    memset(&data_, 0, sizeof(data_));
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void AntBms::init() {
    if (initialized_) return;
    s_antBms = this;
    // init() is retried on every BluetoothBms::update() until it succeeds, so
    // each resource is created once and kept: a retry after a partial failure
    // must not leak the ones that did get created.
    if (stateMutex_ == nullptr) stateMutex_ = xSemaphoreCreateMutex();
    if (connectQueue_ == nullptr) connectQueue_ = xQueueCreate(1, sizeof(uint8_t));
    if (txQueue_ == nullptr) txQueue_ = xQueueCreate(4, sizeof(AntTxFrame));
    if (stateMutex_ == nullptr || connectQueue_ == nullptr || txQueue_ == nullptr) {
        DEBUG_PRINTLN("[ANT] ERROR: mutex or queue create failed");
        return;
    }
    if (!connectTaskStarted_) {
        if (xTaskCreate(connectTask, "ant_conn", 4096, this, 1, nullptr) != pdPASS) {
            DEBUG_PRINTLN("[ANT] ERROR: connect task create failed");
            return;
        }
        connectTaskStarted_ = true;
    }
    if (xTaskCreate(txTask, "ant_tx", 2048, this, 1, nullptr) != pdPASS) {
        DEBUG_PRINTLN("[ANT] ERROR: tx task create failed");
        return;
    }
    state_ = Idle;
    initialized_ = true;
    DEBUG_PRINTLN("[ANT] Init OK");
}

void AntBms::setEnabled(bool enabled) {
    if (!initialized_) return;
    enabled_ = enabled;
    if (!enabled_) {
        resetConnection();
    }
}

void AntBms::setMacAddress(const String& macAddress) {
    macAddress_ = macAddress;
    macAddress_.trim();
}

// ---------------------------------------------------------------------------
// resetConnection
// ---------------------------------------------------------------------------
void AntBms::resetConnection() {
    if (stateMutex_ == nullptr) {
        return;
    }
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    connectSessionId_++;
    applyResetConnectionLocked();
    xSemaphoreGive(stateMutex_);
}

// ---------------------------------------------------------------------------
// applyResetConnectionLocked — caller must hold stateMutex_
// ---------------------------------------------------------------------------
void AntBms::applyResetConnectionLocked() {
    if (pClient_ && pClient_->isConnected()) {
        pClient_->disconnect();
    }
    if (txQueue_ != nullptr) {
        xQueueReset(txQueue_);
    }
    connected_           = false;
    pChar_               = nullptr;
    lastStatusMs_        = 0;
    pendingLen_          = 0;
    assembler_.reset();
    memset(&data_, 0, sizeof(data_));
    state_               = Idle;
}

// ---------------------------------------------------------------------------
// connectTask — runs BLEClient::connect AND service discovery off the main loop
// ---------------------------------------------------------------------------
void AntBms::connectTask(void* arg) {
    AntBms* self = static_cast<AntBms*>(arg);
    uint8_t cmd = 0;

    // Resets state if the attempt for `session` is still the current one.
    auto fail = [self](uint32_t session) {
        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        if (session == self->connectSessionId_) {
            self->applyResetConnectionLocked();
            self->lastConnectAttempt_ = millis();
        }
        xSemaphoreGive(self->stateMutex_);
    };

    for (;;) {
        if (xQueueReceive(self->connectQueue_, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        char mac[18];
        mac[0] = '\0';
        uint32_t session = 0;
        bool okToConnect = false;

        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        session = self->connectSessionId_;
        if (self->macAddress_.length() >= 17) {
            strncpy(mac, self->macAddress_.c_str(), 18);
            mac[17] = '\0';
        }
        okToConnect = self->enabled_ && !throttle.isArmed() && strlen(mac) == 17;
        xSemaphoreGive(self->stateMutex_);

        if (!okToConnect) {
            fail(session);
            continue;
        }

        // ----- Phase 1: BLE connect (PUBLIC first, then RANDOM) -----
        if (self->pClient_ == nullptr) {
            self->pClient_ = BLEDevice::createClient();
        }
        BLEAddress addr(mac);
        bool connectedOk = false;
        if (self->pClient_ != nullptr) {
            applyBmsPreferredConnParams(addr);
            connectedOk = self->pClient_->connect(addr, BLE_ADDR_TYPE_PUBLIC);
            if (!connectedOk) {
                applyBmsPreferredConnParams(addr);
                connectedOk = self->pClient_->connect(addr, BLE_ADDR_TYPE_RANDOM);
            }
        }

        if (!connectedOk) {
            DEBUG_PRINTLN("[ANT] Connection failed (both address types), retrying...");
            fail(session);
            continue;
        }

        // Re-validate before slow ATT operations
        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        bool stillValid = (session == self->connectSessionId_)
            && self->enabled_ && !throttle.isArmed();
        xSemaphoreGive(self->stateMutex_);

        if (!stillValid) {
            if (self->pClient_->isConnected()) self->pClient_->disconnect();
            fail(session);
            continue;
        }

        // ----- Phase 2: ATT service discovery -----
        BLERemoteService* pService = self->pClient_->getService(ANT_SERVICE_UUID);
        if (!pService) {
            DEBUG_PRINTLN("[ANT] Service FFE0 not found");
            self->pClient_->disconnect();
            fail(session);
            continue;
        }

        BLERemoteCharacteristic* pChar = pService->getCharacteristic(ANT_CHAR_UUID);
        if (!pChar) {
            DEBUG_PRINTLN("[ANT] Characteristic FFE1 not found");
            self->pClient_->disconnect();
            fail(session);
            continue;
        }

        pChar->registerForNotify(onAntNotifyCallback);

        // ESP32-C3 needs CCCD written explicitly to enable notifications
        BLERemoteDescriptor* pCccd = pChar->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pCccd) {
            uint8_t notifyOn[2] = {0x01, 0x00};
            pCccd->writeValue(notifyOn, 2, true);
            DEBUG_PRINTLN("[ANT] CCCD written manually (0x0100)");
        } else {
            DEBUG_PRINTLN("[ANT] WARNING: descriptor 0x2902 not found — notify may not work");
        }

        // ----- Phase 3: commit shared state under mutex -----
        xSemaphoreTake(self->stateMutex_, portMAX_DELAY);
        bool sessionStillCurrent = (session == self->connectSessionId_)
            && self->enabled_ && !throttle.isArmed();
        if (!sessionStillCurrent) {
            xSemaphoreGive(self->stateMutex_);
            if (self->pClient_->isConnected()) self->pClient_->disconnect();
            fail(session);
            continue;
        }
        self->pChar_               = pChar;
        self->connected_           = true;
        self->assembler_.reset();
        self->pendingLen_          = 0;
        self->lastRequestMillis_   = 0;
        self->state_               = Subscribed;
        xSemaphoreGive(self->stateMutex_);
        DEBUG_PRINTLN("[ANT] Subscribed, ready for reads");
    }
}

// ---------------------------------------------------------------------------
// txTask — FreeRTOS task that performs BLE writes (write without response)
// ---------------------------------------------------------------------------
void AntBms::txTask(void* arg) {
    AntBms* self = static_cast<AntBms*>(arg);
    AntTxFrame frame;
    for (;;) {
        if (xQueueReceive(self->txQueue_, &frame, portMAX_DELAY)) {
            if (self->pChar_ && self->pClient_ && self->pClient_->isConnected()) {
                self->pChar_->writeValue(frame.data, frame.len, false);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// update — state machine, call from loop()
// ---------------------------------------------------------------------------
void AntBms::update() {
    if (!initialized_) return;
    switch (state_) {

        case Idle: {
            if (!enabled_ || macAddress_.length() < 17) break;
            if (throttle.isArmed()) break;
            {
                const unsigned long now = millis();
                if (lastConnectAttempt_ != 0 && (now - lastConnectAttempt_) < CONNECT_RETRY_MS) break;
            }
            if (connectQueue_ == nullptr) break;
            {
                uint8_t q = 1;
                if (xQueueSend(connectQueue_, &q, 0) != pdTRUE) break;
            }
            DEBUG_PRINTLN("[ANT] Connecting...");
            state_ = Connecting;
            break;
        }

        case Connecting: {
            if (!enabled_ || macAddress_.length() < 17 || throttle.isArmed()) {
                resetConnection();
            }
            break;
        }

        case Subscribed: {
            if (!pClient_->isConnected()) {
                DEBUG_PRINTLN("[ANT] Unexpectedly disconnected");
                resetConnection();
                break;
            }

            processPending();

            const unsigned long now = millis();

            if (lastRequestMillis_ == 0 || (now - lastRequestMillis_) >= REQUEST_INTERVAL_MS) {
                lastRequestMillis_ = now ? now : 1;
                sendStatusRequest();
            }

            if (data_.valid && (now - lastStatusMs_) >= STALE_TIMEOUT_MS) {
                memset(&data_, 0, sizeof(data_));
                DEBUG_PRINTLN("[ANT] Status stale, readings cleared");
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// sendStatusRequest
// ---------------------------------------------------------------------------
void AntBms::sendStatusRequest() {
    if (!txQueue_) return;
    AntTxFrame frame;
    frame.len = antBuildRequest(ANT_FUNC_STATUS, ANT_STATUS_REQ_ADDR, ANT_STATUS_REQ_LEN,
                                frame.data, sizeof(frame.data));
    if (frame.len == 0) return;
    xQueueSend(txQueue_, &frame, 0);
}

// ---------------------------------------------------------------------------
// onNotify — BLE stack task: assemble only, decode later on the loop task
// ---------------------------------------------------------------------------
void AntBms::onNotify(uint8_t* data, size_t len) {
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    // A bad frame (CRC, length, overflow) is simply dropped: the next status
    // request follows within REQUEST_INTERVAL_MS.
    if (assembler_.feed(data, len) == AntFeedResult::Frame) {
        memcpy(pendingFrame_, assembler_.frame(), assembler_.frameLen());
        pendingLen_ = assembler_.frameLen();
    }
    xSemaphoreGive(stateMutex_);
}

// ---------------------------------------------------------------------------
// processPending — loop task: decode the pending frame, if any
// ---------------------------------------------------------------------------
void AntBms::processPending() {
    uint8_t frame[ANT_MAX_FRAME];
    size_t frameLen = 0;

    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    if (pendingLen_ > 0) {
        frameLen = pendingLen_;
        memcpy(frame, pendingFrame_, frameLen);
        pendingLen_ = 0;
    }
    xSemaphoreGive(stateMutex_);

    if (frameLen == 0) return;

    AntBmsData parsed;
    if (!antDecodeStatus(frame, frameLen, &parsed)) return;
    data_ = parsed;
    lastStatusMs_ = millis();
    DEBUG_PRINT("[ANT] V=");
    DEBUG_PRINT(data_.packMv);
    DEBUG_PRINT("mV I=");
    DEBUG_PRINT(data_.packCurrentMa);
    DEBUG_PRINT("mA SoC=");
    DEBUG_PRINT(data_.socPercent);
    DEBUG_PRINT("% cells=");
    DEBUG_PRINTLN(data_.cellCount);
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------
int16_t AntBms::getTempCelsius(uint8_t index) const {
    if (index >= ANT_MAX_PROBES || index >= data_.probeCount) return 0;
    return data_.probeC[index];
}

uint16_t AntBms::getCellVoltageMilliVolts(uint8_t index) const {
    if (index >= ANT_MAX_CELLS || index >= data_.cellCount) return 0;
    return data_.cellMv[index];
}
