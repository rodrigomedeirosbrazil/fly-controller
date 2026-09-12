#pragma once
#include <stdint.h>
#include <string.h>
#include "ControlProtocol.h"

// CRC accumulator, injected so the host test can use its own and the
// firmware can pass esp_rom_crc32_le.
typedef uint32_t (*DfuCrcFn)(uint32_t crc, const uint8_t* data, uint32_t len);

class DfuSession {
public:
    explicit DfuSession(DfuCrcFn crc) : crcFn_(crc) {}

    // Initiate a firmware transfer. Rejects size == 0 or size > DFU_MAX_IMAGE_BYTES.
    // On accept, transitions to Receiving state with received_ = 0 and crc_ = 0.
    // On reject, transitions to Error state. Returns true on accept, false on reject.
    bool begin(uint32_t size, uint32_t expectedCrc) {
        if (size == 0 || size > DFU_MAX_IMAGE_BYTES) {
            state_ = DfuState::Error;
            return false;
        }
        state_ = DfuState::Receiving;
        size_ = size;
        expectedCrc_ = expectedCrc;
        received_ = 0;
        crc_ = 0;
        return true;
    }

    // Accept a packet at the given offset. Returns true only when:
    // - state is Receiving
    // - offset == received_ (exact sequential match)
    // - received_ + len <= size_ (no overshoot)
    // On accept, advances received_ by len. On reject, changes nothing.
    // This implements the "drop out-of-order, resend, and overshoot" policy:
    // the client restarts from received_ on any loss, so buffering would be complexity serving nobody.
    bool acceptOffset(uint32_t offset, uint32_t len) {
        if (state_ != DfuState::Receiving) {
            return false;
        }
        if (offset != received_) {
            return false;  // Gap (offset > received_) or resend (offset < received_)
        }
        // Written as a subtraction, not `received_ + len > size_`: that sum
        // is uint32 and wraps, so a large enough len would pass the check and
        // then corrupt received_. len is bounded by the MTU in practice, but
        // this is a pure function someone may reuse against a different
        // caller. received_ <= size_ is an invariant here, so the subtraction
        // cannot underflow.
        if (len > size_ - received_) {
            return false;  // Would overshoot
        }
        received_ += len;
        return true;
    }

    // Accumulate data into the running CRC. Called after acceptOffset succeeds,
    // usually in a separate loop iteration (staging in a buffer, flushing to flash).
    void accumulate(const uint8_t* data, uint32_t len) {
        crc_ = crcFn_(crc_, data, len);
    }

    // True when the entire image has been received (received_ == size_ && size_ > 0).
    bool isComplete() const {
        return received_ == size_ && size_ > 0;
    }

    // True when the transfer is complete AND the running CRC matches the expected value.
    // This is the gate for marking Ready or committing to flash.
    bool commitAllowed() const {
        return isComplete() && crc_ == expectedCrc_;
    }

    // Transition to Verifying state (checksum validation in progress).
    void markVerifying() {
        state_ = DfuState::Verifying;
    }

    // Transition to Ready state (transfer verified, ready for commit).
    void markReady() {
        state_ = DfuState::Ready;
    }

    // Transition to Error state (validation or I/O failed).
    void markError() {
        state_ = DfuState::Error;
    }

    // Return to Idle state, clearing all transfer state. Safe at any point,
    // including before begin() was ever called. Enables aborting mid-transfer
    // or resetting for a new transfer.
    void abort() {
        state_ = DfuState::Idle;
        size_ = 0;
        expectedCrc_ = 0;
        received_ = 0;
        crc_ = 0;
    }

    // State query (Idle, Receiving, Verifying, Ready, Error).
    DfuState state() const { return state_; }

    // Bytes received so far (advances with each successful acceptOffset).
    uint32_t received() const { return received_; }

    // Total image size for this transfer (set by begin).
    uint32_t size() const { return size_; }

    // Running CRC32 accumulated so far (updated by accumulate).
    uint32_t runningCrc() const { return crc_; }

private:
    DfuCrcFn crcFn_;
    DfuState state_       = DfuState::Idle;
    uint32_t size_        = 0;
    uint32_t expectedCrc_ = 0;
    uint32_t received_    = 0;
    uint32_t crc_         = 0;
};

// ---------------------------------------------------------------------------
// The flash side, implemented in DfuSession.cpp and linked only into the
// firmware. Declared here so BleControl can reach it; the prototypes use no
// Arduino types, so the host test still compiles this header without linking
// the implementation.
//
// The split matters: stage() is called from the Bluedroid task and only
// memcpys, while flush() calls Update.write(), which erases a flash block on
// boundaries and blocks for ~150 ms. That must happen on the loop task.
// ---------------------------------------------------------------------------
namespace DfuFlash {
    bool     begin(uint32_t size);
    bool     hasRoom(uint32_t len);
    bool     stage(const uint8_t* data, uint32_t len);
    bool     flush();
    uint32_t stagedBytes();
    bool     finish();
    void     abort();
}
