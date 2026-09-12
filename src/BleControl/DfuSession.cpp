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
//    - BLE callback (Bluedroid task) stages bytes into a FreeRTOS stream buffer via stage()
//    - Arduino loop() task flushes from the stream buffer via flush() when ready
//    - This defers the blocking erase to a safe context
//
// 3. The stream buffer (16 KB) absorbs the burst of packets that arrive while
//    a sector erase blocks, preventing data loss when Update.write() stalls
//    for ~150 ms. Flush processes 4 KB at a time, one chunk per loop iteration,
//    to preserve the timing budget for throttle and ESC control.
//
// 4. Data-race fix: stream and retired are guarded by a lifecycle protocol.
//    abort() detaches the stream (setting it to nullptr) and hands it to the
//    retired slot. The Bluedroid task stops seeing it immediately. releaseIfRetired()
//    frees it a full loop iteration later (after the quarantine flag), by which
//    time any callback that had already read the stream handle has returned.
//
#include "DfuSession.h"
#include <esp_rom_crc.h>
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

namespace DfuFlash {

static const size_t kStreamBytes = 16 * 1024;   // absorbs the burst that
                                                // arrives while a sector erases
static const size_t kFlushChunk  = 4096;

// Read by BOTH tasks. Detaching it (setting it to nullptr) is how the loop
// task tells the Bluedroid task to stop touching the stream.
static StreamBufferHandle_t stream = nullptr;

// Loop task only: a handle that has been detached and is waiting out its
// quarantine before being freed.
static StreamBufferHandle_t retired = nullptr;
static bool retiredSeen = false;

// Detach the stream so the Bluedroid task stops seeing it, and hand it to the
// retired slot. It is NOT freed here: abort() runs on the loop task while a
// BLE callback may be part-way through stage(), so freeing immediately would
// be a use-after-free. releaseIfRetired() frees it a full loop iteration
// later, by which time any callback that had already read the handle has
// returned.
static void retire() {
    if (stream != nullptr) {
        retired     = stream;
        stream      = nullptr;
        retiredSeen = false;
    }
}

// Begin an OTA update. Returns false if Update.begin() fails (e.g., partition
// error or size validation) or stream buffer allocation fails.
// This call is fast and does not erase.
bool begin(uint32_t size) {
    abort();                      // clean slate; safe if nothing is running

    // Reclaim a handle still serving out its quarantine rather than
    // allocating a second one.
    //
    // This is not an optimisation, it closes a leak. `retired` is a single
    // slot, and drainQueue() empties the whole four-deep request queue in one
    // handle() call -- so ABORT/BEGIN/ABORT/BEGIN all execute before
    // serviceDfu() runs once, and the second retire would overwrite the first
    // handle with no one left holding a pointer to free it. Every transfer
    // the app starts opens with exactly that ABORT then BEGIN pair.
    //
    // Handing the same buffer straight back is also safer than a fresh one:
    // anything still holding this pointer is holding a LIVE buffer, which is
    // the entire purpose of the quarantine. Reset clears whatever the
    // abandoned transfer left behind. With this, at most one 16 KB buffer
    // ever exists, and `retired` can never be overwritten -- a second
    // abort() finds `stream` already null and does nothing.
    if (retired != nullptr) {
        stream      = retired;
        retired     = nullptr;
        retiredSeen = false;
        xStreamBufferReset(stream);
    } else {
        stream = xStreamBufferCreate(kStreamBytes, 1);
    }

    if (stream == nullptr) {
        Serial.printf("[DfuFlash] stream buffer alloc failed, free heap %u\n",
                      (unsigned) ESP.getFreeHeap());
        return false;
    }
    if (!Update.begin(size)) {
        retire();
        return false;
    }
    return true;
}

// Return true if the stream has room for at least len bytes.
// Null-guarded: returns false if stream has been detached.
bool hasRoom(uint32_t len) {
    return stream != nullptr && xStreamBufferSpacesAvailable(stream) >= len;
}

// Stage incoming data into the stream buffer. Returns true if the data was
// successfully accepted by the stream. Returns false if stream is detached or
// the stream has no room. When false, the caller drops the packet
// (documented recovery: the client restarts from the accepted offset,
// resending the dropped packet).
bool stage(const uint8_t* data, uint32_t len) {
    if (stream == nullptr) {
        return false;
    }
    return xStreamBufferSend(stream, data, len, 0) == len;
}

// Return the number of bytes currently staged in the stream (not yet flushed).
// Null-guarded: returns 0 if stream has been detached.
uint32_t stagedBytes() {
    return (stream == nullptr) ? 0 : (uint32_t) xStreamBufferBytesAvailable(stream);
}

// Flush a chunk from the stream buffer to flash via Update.write(). This is
// the call that blocks for ~150 ms per 64 KB erased. Should only be called
// from the Arduino loop() task, not from a BLE/Bluedroid callback.
// Processes one 4 KB chunk per call, not a loop until empty: every Update.write()
// can block on a sector erase, and loop() owes the throttle and the ESC their
// timing. Absorbing the burst is the stream buffer's job, not this function's.
// Returns false if the write fails or stream is detached. On false, the transfer
// should be aborted. Returns true if nothing was staged (nothing to write).
bool flush() {
    if (stream == nullptr) {
        return true;   // nothing staged, nothing to fail
    }

    // static, NOT a stack local: the Arduino loop task has an 8 KB stack and
    // this is 4 KB of it. Only the loop task calls flush(), so file scope is
    // safe.
    static uint8_t chunk[kFlushChunk];

    const size_t got = xStreamBufferReceive(stream, chunk, sizeof(chunk), 0);
    if (got == 0) {
        return true;
    }

    // A SHORT write is a failure, not a success. Returning true on `written >
    // 0` would let a partial flash write pass while the caller's accepted
    // offset and running CRC moved on for the full amount -- the transfer
    // would then fail at commit with no indication of where it went wrong.
    return Update.write(chunk, got) == got;
}

// Complete the OTA update and verify. Finalizes the OTA partition and
// marks it ready for the bootloader to use. Returns false if finalization fails.
// Detaches the stream buffer after commit to begin quarantine.
bool finish() {
    const bool ok = Update.end(true);
    retire();
    return ok;
}

// Abort the OTA update and detach the stream buffer to begin quarantine.
// Does NOT free immediately; releaseIfRetired() will free on the next iteration.
void abort() {
    Update.abort();
    retire();
}

// Free the retired stream buffer after the quarantine window. Must be called
// once per loop iteration from the Arduino loop() task. Detaching in retire()
// followed by delayed freeing here prevents a use-after-free when abort() runs
// on the loop task while a Bluedroid callback is mid-flight in stage().
void releaseIfRetired() {
    if (retired == nullptr) {
        return;
    }
    if (!retiredSeen) {
        retiredSeen = true;   // quarantine: free on a later tick, not this one
        return;
    }
    vStreamBufferDelete(retired);
    retired     = nullptr;
    retiredSeen = false;
}

}  // namespace DfuFlash
