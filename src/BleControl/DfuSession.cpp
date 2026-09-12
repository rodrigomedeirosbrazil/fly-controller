// DFU flash staging and write operations.
//
// KEY ARCHITECTURE NOTES:
//
// 1. Update.begin(size) does NOT erase the OTA partition — it only resolves
//    the partition metadata and resets the write state. Therefore, begin()
//    can run inline on any task (Bluedroid or loop) without blocking.
//
// 2. Update.write() is what BLOCKS — it erases the next 64 KB block as data
//    arrives, roughly 150 ms per block. It MUST NOT run on the Bluedroid task
//    (the BLE callback's task) because blocking there starves the radio and
//    causes packet loss or disconnections. Instead:
//    - BLE callback (Bluedroid task) stages bytes into a 4096-byte buffer via stage()
//    - Arduino loop() task flushes the buffer via flush() when ready
//    - This defers the blocking erase to a safe context
//
// 3. The 4096-byte staging buffer matches a flash sector, balancing buffer
//    memory and the number of flush cycles (roughly every 4 KB received).
//
#include "DfuSession.h"
#include <esp_rom_crc.h>
#include <Update.h>

namespace DfuFlash {

// 4096-byte staging buffer for received image data.
static uint8_t stageBuffer[4096];
static uint32_t stageCursor = 0;

// Begin an OTA update. Returns false if Update.begin() fails (e.g., partition
// error or size validation). This call is fast and does not erase.
bool begin(uint32_t size) {
    stageCursor = 0;
    return Update.begin(size);
}

// Stage incoming data into the buffer. Returns true if the data fits,
// false if the buffer is full. When false, the caller drops the packet
// (documented recovery: the client restarts from the accepted offset,
// resending the dropped packet).
bool hasRoom(uint32_t len) {
    // Subtraction, not `stageCursor + len > sizeof(...)`: that sum is uint32
    // and wraps, so a large enough len would report room that does not exist.
    return len <= sizeof(stageBuffer) - stageCursor;
}

bool stage(const uint8_t* data, uint32_t len) {
    if (!hasRoom(len)) {
        return false;
    }
    memcpy(stageBuffer + stageCursor, data, len);
    stageCursor += len;
    return true;
}

// Flush the staging buffer to flash via Update.write(). This is the call
// that blocks for ~150 ms per 64 KB erased. Should only be called from
// the Arduino loop() task, not from a BLE/Bluedroid callback.
// Returns false if the write fails. On false, the transfer should be aborted.
bool flush() {
    if (stageCursor == 0) {
        return true;  // Nothing to write
    }
    // A SHORT write is a failure, not a success. Returning true on `written >
    // 0` would let a partial flash write pass while the caller's accepted
    // offset and running CRC moved on for the full amount -- the transfer
    // would then fail at commit with no indication of where it went wrong.
    // The cursor is cleared only after the count is compared, so a failure
    // does not also silently discard the bytes.
    const size_t pending = stageCursor;
    const size_t written = Update.write(stageBuffer, pending);
    if (written != pending) {
        return false;
    }
    stageCursor = 0;
    return true;
}

// Return the number of bytes currently staged (not yet flushed).
uint32_t stagedBytes() {
    return stageCursor;
}

// Complete the OTA update and verify. Finalizes the OTA partition and
// marks it ready for the bootloader to use. Returns false if finalization fails.
bool finish() {
    return Update.end(true);
}

// Abort the OTA update and clear the staging buffer.
void abort() {
    Update.abort();
    stageCursor = 0;
}

}  // namespace DfuFlash
