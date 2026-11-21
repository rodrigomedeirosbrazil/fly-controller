#include "AntBms.h"
#include "../config.h"
#include "../Throttle/Throttle.h"
#include "../Version.h"
#include <cstring>
#include <cstdio>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_gap_ble_api.h>
#include "../BluetoothBms/BmsLinkParams.h"
#if ANT_BMS_DIAG
#include "../Logger/LogStore.h"
#endif

struct AntTxFrame {
    uint8_t data[ANT_REQUEST_SIZE];
    size_t  len;
};

static AntBms* s_antBms = nullptr;

#if ANT_BMS_DIAG
char AntBms::diagPending_[ANT_DIAG_PENDING_CAP];
#endif

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
      txQueue_(nullptr), txTaskHandle_(nullptr),
      connectQueue_(nullptr), connectTaskHandle_(nullptr), stateMutex_(nullptr),
      connectSessionId_(0),
      state_(Idle), initialized_(false), enabled_(false), connected_(false),
      lastConnectAttempt_(0), lastRequestMillis_(0), lastStatusMs_(0),
      statusSeen_(false), deviceInfoRequested_(false),
      pendingLen_(0),
      cntOk_(0), cntCrc_(0), cntLen_(0), cntOvf_(0), cntOverwritten_(0),
      errKind_(0), errLen_(0),
      statStale_(0), lastStatsMs_(0)
#if ANT_BMS_DIAG
      , diagMutex_(nullptr), diagStarted_(false), lastDiagFlushMs_(0), lastRawLogMs_(0),
      subscribedAtMs_(0), firstStatusLogged_(false), statusLogged_(0)
#endif
{
    memset(hwVersion_, 0, sizeof(hwVersion_));
    memset(swVersion_, 0, sizeof(swVersion_));
    memset(pendingFrame_, 0, sizeof(pendingFrame_));
    memset(errBuf_, 0, sizeof(errBuf_));
    memset(&data_, 0, sizeof(data_));
}

// ---------------------------------------------------------------------------
// Diagnostics (compiled only with -DANT_BMS_DIAG=1)
// ---------------------------------------------------------------------------
#if ANT_BMS_DIAG
void AntBms::diagLine(const char* text) {
    if (diagMutex_ == nullptr) return;
    xSemaphoreTake(diagMutex_, portMAX_DELAY);
    diag_.addLine((uint32_t)millis(), text);
    xSemaphoreGive(diagMutex_);
}

void AntBms::diagHex(const char* prefix, const uint8_t* data, size_t len) {
    char line[ANT_DIAG_LINE_MAX];
    int p = snprintf(line, sizeof(line), "%s", prefix);
    if (p < 0 || (size_t)p >= sizeof(line)) return;
    antFormatHex(data, len, line + p, sizeof(line) - (size_t)p);
    diagLine(line);
}

void AntBms::diagLogStatus(const uint8_t* frame, size_t len) {
    const unsigned long now = millis();
    const bool logRaw = statusLogged_ < DIAG_RAW_FIRST_COUNT
        || (now - lastRawLogMs_) >= DIAG_RAW_PERIOD_MS;
    if (!logRaw) return;
    statusLogged_++;
    lastRawLogMs_ = now;
    diagHex("status raw ", frame, len);

    char line[ANT_DIAG_LINE_MAX];
    int n = snprintf(line, sizeof(line),
        "status C=%u T=%u mv=%lu rawI=%d ma=%ld soc=%u capmah=%lu min=%u max=%u delta=%u "
        "dmos=%u cmos=%u bstat=%u perm=%u probes=",
        data_.cellCount, data_.probeCount, (unsigned long)data_.packMv,
        (int)data_.rawCurrentDeciA, (long)data_.packCurrentMa, data_.socPercent,
        (unsigned long)data_.nominalCapacityMah, data_.cellMinMv, data_.cellMaxMv,
        data_.cellDeltaMv, data_.dischargeMos, data_.chargeMos, data_.batteryStatus,
        data_.permission);
    if (n < 0 || (size_t)n >= sizeof(line)) return;
    size_t pos = (size_t)n;
    for (uint8_t i = 0; i < data_.probeCount && pos + 8 < sizeof(line); i++) {
        n = snprintf(line + pos, sizeof(line) - pos, "%s%d", i ? "," : "", (int)data_.probeC[i]);
        if (n < 0) break;
        pos += (size_t)n;
    }
    if (pos + 1 < sizeof(line)) {
        snprintf(line + pos, sizeof(line) - pos, " mos=%d bal=%d",
                 (int)data_.mosC, (int)data_.balancerC);
    }
    diagLine(line);
}

void AntBms::diagFlush() {
    const unsigned long now = millis();
    if (diagStarted_ && (now - lastDiagFlushMs_) < DIAG_FLUSH_INTERVAL_MS) return;
    lastDiagFlushMs_ = now;

    const size_t nameLen = strlen(ANT_DIAG_FILE_NAME);
    if (!diagStarted_) {
        uint32_t size = 0;
        const bool exists = LogStore::fileSize(ANT_DIAG_FILE_NAME, nameLen, size);
        xSemaphoreTake(diagMutex_, portMAX_DELAY);
        if (exists && size > 0) {
            static const char bootMark[] = "# ---- boot ----\n";
            const size_t markLen = sizeof(bootMark) - 1;
            if (LogStore::append(ANT_DIAG_FILE_NAME, nameLen,
                                 reinterpret_cast<const uint8_t*>(bootMark), markLen)) {
                size += (uint32_t)markLen;
            }
            diag_.setExistingFileBytes(size);
        }
        xSemaphoreGive(diagMutex_);
        diagStarted_ = true;
    }

    xSemaphoreTake(diagMutex_, portMAX_DELAY);
    size_t n = diag_.takePending(diagPending_, ANT_DIAG_PENDING_CAP);
    xSemaphoreGive(diagMutex_);
    if (n == 0) return;

    if (LogStore::append(ANT_DIAG_FILE_NAME, nameLen,
                         reinterpret_cast<const uint8_t*>(diagPending_), n)) {
        xSemaphoreTake(diagMutex_, portMAX_DELAY);
        diag_.commitWritten(n);
        xSemaphoreGive(diagMutex_);
    }
}
#endif

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void AntBms::init() {
    if (initialized_) return;
    s_antBms = this;
    stateMutex_ = xSemaphoreCreateMutex();
    connectQueue_ = xQueueCreate(1, sizeof(uint8_t));
    if (stateMutex_ == nullptr || connectQueue_ == nullptr) {
        DEBUG_PRINTLN("[ANT] ERROR: mutex or connect queue create failed");
        return;
    }
#if ANT_BMS_DIAG
    diagMutex_ = xSemaphoreCreateMutex();
    if (diagMutex_ != nullptr) {
        diag_.begin(0);
        char line[96];
        snprintf(line, sizeof(line), "diag start fw=%s type=ant mac=%s",
                 APP_VERSION, macAddress_.c_str());
        diagLine(line);
    }
#endif
    if (xTaskCreate(connectTask, "ant_conn", 4096, this, 1, &connectTaskHandle_) != pdPASS) {
        DEBUG_PRINTLN("[ANT] ERROR: connect task create failed");
        return;
    }
    txQueue_ = xQueueCreate(4, sizeof(AntTxFrame));
    xTaskCreate(txTask, "ant_tx", 2048, this, 1, &txTaskHandle_);
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
#if ANT_BMS_DIAG
    if (connected_) {
        char line[48];
        snprintf(line, sizeof(line), "disconnect after %lu ms",
                 (unsigned long)(millis() - subscribedAtMs_));
        diagLine(line);
    }
#endif
    if (pClient_ && pClient_->isConnected()) {
        pClient_->disconnect();
    }
    if (txQueue_ != nullptr) {
        xQueueReset(txQueue_);
    }
    connected_           = false;
    pChar_               = nullptr;
    deviceInfoRequested_ = false;
    statusSeen_          = false;
    lastStatusMs_        = 0;
    pendingLen_          = 0;
    errKind_             = 0;
    errLen_              = 0;
    assembler_.reset();
    memset(hwVersion_, 0, sizeof(hwVersion_));
    memset(swVersion_, 0, sizeof(swVersion_));
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
#if ANT_BMS_DIAG
            self->diagLine(connectedOk ? "connect public ok" : "connect public fail");
#endif
            if (!connectedOk) {
                applyBmsPreferredConnParams(addr);
                connectedOk = self->pClient_->connect(addr, BLE_ADDR_TYPE_RANDOM);
#if ANT_BMS_DIAG
                self->diagLine(connectedOk ? "connect random ok" : "connect random fail");
#endif
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
#if ANT_BMS_DIAG
            self->diagLine("service ffe0 missing");
#endif
            self->pClient_->disconnect();
            fail(session);
            continue;
        }

        BLERemoteCharacteristic* pChar = pService->getCharacteristic(ANT_CHAR_UUID);
        if (!pChar) {
            DEBUG_PRINTLN("[ANT] Characteristic FFE1 not found");
#if ANT_BMS_DIAG
            self->diagLine("char ffe1 missing");
#endif
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
#if ANT_BMS_DIAG
            self->diagLine("cccd ok");
#endif
        } else {
            DEBUG_PRINTLN("[ANT] WARNING: descriptor 0x2902 not found — notify may not work");
#if ANT_BMS_DIAG
            self->diagLine("cccd missing");
#endif
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
        self->errKind_             = 0;
        self->lastRequestMillis_   = 0;
        self->statusSeen_          = false;
        self->deviceInfoRequested_ = false;
#if ANT_BMS_DIAG
        self->subscribedAtMs_      = millis();
        self->firstStatusLogged_   = false;
#endif
        self->state_               = Subscribed;
        xSemaphoreGive(self->stateMutex_);
        DEBUG_PRINTLN("[ANT] Subscribed, ready for reads");
#if ANT_BMS_DIAG
        self->diagLine("subscribed");
#endif
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

            if (!deviceInfoRequested_) {
                deviceInfoRequested_ = true;
                sendRequest(ANT_FUNC_DEVICE_INFO, ANT_DEVINFO_REQ_ADDR, ANT_DEVINFO_REQ_LEN);
            }

            if (lastRequestMillis_ == 0 || (now - lastRequestMillis_) >= REQUEST_INTERVAL_MS) {
                lastRequestMillis_ = now ? now : 1;
                sendRequest(ANT_FUNC_STATUS, ANT_STATUS_REQ_ADDR, ANT_STATUS_REQ_LEN);
            }

            if (statusSeen_ && data_.valid && (now - lastStatusMs_) >= STALE_TIMEOUT_MS) {
                memset(&data_, 0, sizeof(data_));
                statStale_++;
                DEBUG_PRINTLN("[ANT] Status stale, readings cleared");
            }
            break;
        }
    }

#if ANT_BMS_DIAG
    if (state_ == Subscribed) {
        const unsigned long now = millis();
        if ((now - lastStatsMs_) >= DIAG_STATS_PERIOD_MS) {
            lastStatsMs_ = now;
            uint32_t ok, crc, len, ovf;
            xSemaphoreTake(stateMutex_, portMAX_DELAY);
            ok = cntOk_; crc = cntCrc_; len = cntLen_; ovf = cntOvf_;
            xSemaphoreGive(stateMutex_);
            char line[96];
            snprintf(line, sizeof(line), "stats ok=%lu crc=%lu len=%lu ovf=%lu stale=%lu",
                     (unsigned long)ok, (unsigned long)crc, (unsigned long)len,
                     (unsigned long)ovf, (unsigned long)statStale_);
            diagLine(line);
        }
    }
    if (diagMutex_ != nullptr) diagFlush();
#endif
}

// ---------------------------------------------------------------------------
// sendRequest
// ---------------------------------------------------------------------------
void AntBms::sendRequest(uint8_t func, uint16_t addr, uint8_t len) {
    if (!txQueue_) return;
    AntTxFrame frame;
    frame.len = antBuildRequest(func, addr, len, frame.data, sizeof(frame.data));
    if (frame.len == 0) return;
    DEBUG_PRINT("[ANT] TX func=0x");
    DEBUG_PRINT_HEX(func, HEX);
    DEBUG_PRINTLN();
#if ANT_BMS_DIAG
    diagHex("tx ", frame.data, frame.len);
#endif
    xQueueSend(txQueue_, &frame, 0);
}

// ---------------------------------------------------------------------------
// onNotify — BLE stack task: assemble only, decode later on the loop task
// ---------------------------------------------------------------------------
void AntBms::onNotify(uint8_t* data, size_t len) {
    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    AntFeedResult r = assembler_.feed(data, len);
    switch (r) {
        case AntFeedResult::Frame:
            if (pendingLen_ != 0) cntOverwritten_++;
            memcpy(pendingFrame_, assembler_.frame(), assembler_.frameLen());
            pendingLen_ = assembler_.frameLen();
            break;
        case AntFeedResult::CrcError:
        case AntFeedResult::LengthError:
        case AntFeedResult::Overflow:
            if (r == AntFeedResult::CrcError) cntCrc_++;
            else if (r == AntFeedResult::LengthError) cntLen_++;
            else cntOvf_++;
            errKind_ = (uint8_t)r;
            errLen_ = assembler_.bufferLen();
            memcpy(errBuf_, assembler_.buffer(), errLen_);
            break;
        case AntFeedResult::None:
            break;
    }
    xSemaphoreGive(stateMutex_);
}

// ---------------------------------------------------------------------------
// processPending — loop task: decode the pending frame, if any
// ---------------------------------------------------------------------------
void AntBms::processPending() {
    uint8_t frame[ANT_MAX_FRAME];
    size_t frameLen = 0;
    uint8_t errKind = 0;
#if ANT_BMS_DIAG
    uint8_t errBuf[ANT_MAX_FRAME];
    size_t errLen = 0;
    uint32_t overwritten = 0;
#endif

    xSemaphoreTake(stateMutex_, portMAX_DELAY);
    if (pendingLen_ > 0) {
        frameLen = pendingLen_;
        memcpy(frame, pendingFrame_, frameLen);
        pendingLen_ = 0;
        cntOk_++;
    }
    errKind = errKind_;
#if ANT_BMS_DIAG
    if (errKind != 0) {
        errLen = errLen_;
        memcpy(errBuf, errBuf_, errLen);
    }
    overwritten = cntOverwritten_;
    cntOverwritten_ = 0;
#endif
    errKind_ = 0;
    xSemaphoreGive(stateMutex_);

#if ANT_BMS_DIAG
    if (errKind != 0) {
        const char* prefix = errKind == (uint8_t)AntFeedResult::CrcError ? "asm crc "
            : errKind == (uint8_t)AntFeedResult::LengthError ? "asm length " : "asm overflow ";
        diagHex(prefix, errBuf, errLen);
    }
    for (uint32_t i = 0; i < overwritten && i < 4; i++) diagLine("frame overwritten");
#else
    (void)errKind;
#endif

    if (frameLen == 0) return;

    if (frame[2] == ANT_REPLY_STATUS) {
        AntBmsData parsed;
        if (antDecodeStatus(frame, frameLen, &parsed)) {
            data_ = parsed;
            lastStatusMs_ = millis();
            statusSeen_ = true;
            DEBUG_PRINT("[ANT] V=");
            DEBUG_PRINT(data_.packMv);
            DEBUG_PRINT("mV I=");
            DEBUG_PRINT(data_.packCurrentMa);
            DEBUG_PRINT("mA SoC=");
            DEBUG_PRINT(data_.socPercent);
            DEBUG_PRINT("% cells=");
            DEBUG_PRINTLN(data_.cellCount);
#if ANT_BMS_DIAG
            if (!firstStatusLogged_) {
                firstStatusLogged_ = true;
                char line[48];
                snprintf(line, sizeof(line), "first status after %lu ms",
                         (unsigned long)(millis() - subscribedAtMs_));
                diagLine(line);
            }
            diagLogStatus(frame, frameLen);
#endif
        }
    } else if (frame[2] == ANT_REPLY_DEVICE_INFO) {
        char hw[17], sw[17];
        if (antDecodeDeviceInfo(frame, frameLen, hw, sw)) {
            memcpy(hwVersion_, hw, sizeof(hwVersion_));
            memcpy(swVersion_, sw, sizeof(swVersion_));
            DEBUG_PRINT("[ANT] HW ");
            DEBUG_PRINT(hwVersion_);
            DEBUG_PRINT(" SW ");
            DEBUG_PRINTLN(swVersion_);
#if ANT_BMS_DIAG
            diagHex("devinfo raw ", frame, frameLen);
            char line[64];
            snprintf(line, sizeof(line), "devinfo hw=%s sw=%s", hwVersion_, swVersion_);
            diagLine(line);
#endif
        }
    }
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
