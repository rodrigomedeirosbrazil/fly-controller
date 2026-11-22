#ifndef LOG_STORE_H
#define LOG_STORE_H

#include <Arduino.h>
#include "LogListing.h"

// The only code that touches LittleFS on behalf of the log opcodes and the
// Logger's retention. Every call opens and closes what it uses: nothing is
// held across requests, so a phone that disconnects mid-download leaks
// nothing. Names are passed WITHOUT the leading '/'.
namespace LogStore {
    uint32_t usedBytes();
    uint32_t totalBytes();

    // Offers every regular file in the root to the builder.
    void list(LogPageBuilder& builder);

    // False if the file does not exist.
    bool fileSize(const char* name, size_t len, uint32_t& size);

    // Reads up to `len` bytes at `offset`. Returns the count read.
    size_t read(const char* name, size_t nameLen, uint32_t offset, uint8_t* buf, size_t len);

    // False if the file does not exist.
    bool remove(const char* name, size_t len);

    // Removes every .csv/.txt in the root. Returns how many were removed.
    uint16_t removeAll();

    // Deletes the oldest deletable file (LogRetentionPicker) while free space
    // is below `reserveBytes`, at most `maxDeletions` times. Never touches
    // `current`. Returns how many were deleted.
    uint8_t rotate(const char* current, size_t currentLen,
                   uint32_t reserveBytes, uint8_t maxDeletions);
}

#endif // LOG_STORE_H
