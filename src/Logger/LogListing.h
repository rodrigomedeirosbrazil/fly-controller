#ifndef LOG_LISTING_H
#define LOG_LISTING_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "../BleControl/ControlProtocol.h"

// Pure decisions about flight-log files: which names exist on the wire, how
// LOG_LIST pages them, and which file retention deletes. No Arduino, no
// LittleFS -- LogStore feeds directory entries in and does the I/O.

// The rule fly-app's isValidLogName() applies too: 1-24 bytes, every byte
// printable ASCII except space (0x21-0x7E), no '/', no "..", ending in
// ".csv" or ".txt". Names never carry the leading '/'.
inline bool hasLogExtension(const char* name, size_t len) {
    if (name == nullptr || len < 4) {
        return false;
    }
    return memcmp(name + len - 4, ".csv", 4) == 0 ||
           memcmp(name + len - 4, ".txt", 4) == 0;
}

inline bool isValidLogName(const char* name, size_t len) {
    if (name == nullptr || len < 1 || len > LOG_MAX_NAME_LEN) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        const uint8_t c = (uint8_t) name[i];
        if (c <= 0x20 || c >= 0x7F || c == '/') {
            return false;
        }
        if (c == '.' && i + 1 < len && name[i + 1] == '.') {
            return false;
        }
    }
    return hasLogExtension(name, len);
}

// Byte-wise, then shorter first. Only has to be consistent with itself: the
// client sends back the last name it received as the next cursor.
inline int compareLogNames(const char* a, size_t aLen, const char* b, size_t bLen) {
    const size_t n = aLen < bLen ? aLen : bLen;
    const int c = memcmp(a, b, n);
    if (c != 0) {
        return c;
    }
    return (aLen < bLen) ? -1 : (aLen > bLen ? 1 : 0);
}

inline uint32_t logFreeBytes(uint32_t total, uint32_t used) {
    return total > used ? total - used : 0;
}

// More than the most entries a 240-byte page can carry. Only two valid names
// are 4 bytes long (".csv", ".txt": 9-byte entries); every other one is at
// least 5 (10-byte entries), so 228 payload bytes hold at most 2 + 21 = 23.
#define LOG_PAGE_KEEP 24
#define LOG_LIST_HEADER 12   // [used u32][total u32][fileCount u16][flags u8][n u8]
#define LOG_LIST_FLAG_MORE 0x01

// Builds one LOG_LIST page from directory entries offered in ANY order
// (LittleFS iteration order is not sorted). Keeps only the LOG_PAGE_KEEP
// smallest listable names strictly greater than the cursor, so memory is
// fixed however many files exist. Cursor paging, not index paging: a delete
// between pages cannot skip or repeat a file.
class LogPageBuilder {
public:
    void reset(const char* cursor, size_t cursorLen) {
        cursorLen_ = (cursor != nullptr && cursorLen <= LOG_MAX_NAME_LEN) ? (uint8_t) cursorLen : 0;
        if (cursorLen_ > 0) {
            memcpy(cursor_, cursor, cursorLen_);
        }
        keptCount_   = 0;
        fileCount_   = 0;
        afterCursor_ = 0;
    }

    // Listable = valid name and non-empty. Zero-byte files are the Logger's
    // pre-created placeholders and are hidden.
    void offer(const char* name, size_t len, uint32_t size) {
        if (size == 0 || !isValidLogName(name, len)) {
            return;
        }
        if (fileCount_ < 0xFFFF) {
            fileCount_++;
        }
        if (cursorLen_ > 0 && compareLogNames(name, len, cursor_, cursorLen_) <= 0) {
            return;
        }
        afterCursor_++;
        insertSorted(name, (uint8_t) len, size);
    }

    uint16_t fileCount() const { return fileCount_; }

    // Returns the payload length, or 0 when `limit` cannot hold the header
    // plus the first pending entry -- a page that could never advance.
    size_t encode(uint8_t* out, size_t limit, uint32_t usedBytes, uint32_t totalBytes) const {
        if (out == nullptr || limit < LOG_LIST_HEADER) {
            return 0;
        }
        size_t o = LOG_LIST_HEADER;
        uint8_t emitted = 0;
        for (; emitted < keptCount_; emitted++) {
            const Kept& k = kept_[emitted];
            if (o + 5 + k.len > limit) {
                break;
            }
            memcpy(out + o, &k.size, 4);
            out[o + 4] = k.len;
            memcpy(out + o + 5, k.name, k.len);
            o += 5 + k.len;
        }
        if (emitted == 0 && afterCursor_ > 0) {
            return 0;
        }
        memcpy(out, &usedBytes, 4);
        memcpy(out + 4, &totalBytes, 4);
        memcpy(out + 8, &fileCount_, 2);
        out[10] = (afterCursor_ > emitted) ? LOG_LIST_FLAG_MORE : 0;
        out[11] = emitted;
        return o;
    }

private:
    struct Kept {
        char     name[LOG_MAX_NAME_LEN];
        uint8_t  len;
        uint32_t size;
    };

    void insertSorted(const char* name, uint8_t len, uint32_t size) {
        uint8_t pos = keptCount_;
        while (pos > 0 && compareLogNames(name, len, kept_[pos - 1].name, kept_[pos - 1].len) < 0) {
            pos--;
        }
        if (pos >= LOG_PAGE_KEEP) {
            return;   // larger than everything kept and the array is full
        }
        const uint8_t last = keptCount_ < LOG_PAGE_KEEP ? keptCount_ : (uint8_t) (LOG_PAGE_KEEP - 1);
        for (uint8_t i = last; i > pos; i--) {
            kept_[i] = kept_[i - 1];
        }
        memcpy(kept_[pos].name, name, len);
        kept_[pos].len  = len;
        kept_[pos].size = size;
        if (keptCount_ < LOG_PAGE_KEEP) {
            keptCount_++;
        }
    }

    char     cursor_[LOG_MAX_NAME_LEN] = {};
    uint8_t  cursorLen_   = 0;
    Kept     kept_[LOG_PAGE_KEEP];
    uint8_t  keptCount_   = 0;
    uint16_t fileCount_   = 0;
    uint16_t afterCursor_ = 0;
};

// Retention: the oldest file that deleting would actually free space from.
// "Oldest" is the smallest name. Dated names (YYYYMMDD_NNN) sort in time
// order; clockless NNNNN names sort before every dated one and so go first
// regardless of real age -- LittleFS keeps no trustworthy mtime without a
// clock. Never the file the Logger is writing.
class LogRetentionPicker {
public:
    void reset(const char* current, size_t currentLen) {
        currentLen_ = (current != nullptr && currentLen <= LOG_MAX_NAME_LEN) ? (uint8_t) currentLen : 0;
        if (currentLen_ > 0) {
            memcpy(current_, current, currentLen_);
        }
        pickLen_ = 0;
        pick_[0] = '\0';
    }

    void offer(const char* name, size_t len, uint32_t size) {
        if (size == 0 || !isValidLogName(name, len)) {
            return;
        }
        if (currentLen_ > 0 && compareLogNames(name, len, current_, currentLen_) == 0) {
            return;
        }
        if (pickLen_ == 0 || compareLogNames(name, len, pick_, pickLen_) < 0) {
            memcpy(pick_, name, len);
            pick_[len] = '\0';
            pickLen_   = (uint8_t) len;
        }
    }

    bool        hasPick() const { return pickLen_ > 0; }
    const char* pick()    const { return pick_; }
    uint8_t     pickLen() const { return pickLen_; }

private:
    char    current_[LOG_MAX_NAME_LEN] = {};
    uint8_t currentLen_ = 0;
    char    pick_[LOG_MAX_NAME_LEN + 1] = {};
    uint8_t pickLen_ = 0;
};

#endif // LOG_LISTING_H
