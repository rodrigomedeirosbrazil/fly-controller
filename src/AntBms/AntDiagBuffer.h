#pragma once

// Pure, bounded text-line buffer for the ANT BMS on-flash diagnostic capture.
// The loop task takes pending bytes, appends them to /antdiag.txt through
// LogStore::append and reports how many were written. No Arduino
// dependencies, so it is host-tested (test/AntDiagBufferTest.cpp).

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ANT_DIAG_FILE_NAME "antdiag.txt"
#define ANT_DIAG_FILE_CAP 49152
#define ANT_DIAG_PENDING_CAP 6144
#define ANT_DIAG_TRUNCATED_MARK "# truncated\n"
#define ANT_DIAG_TRUNCATED_LEN 12
#define ANT_DIAG_LINE_MAX 640  // fits a 256-byte buffer as hex (512) plus prefix and text

// Lowercase hex pairs, no separator. Always NUL-terminates (when outMax > 0)
// and truncates on a whole byte. Returns the chars written.
inline size_t antFormatHex(const uint8_t* data, size_t len, char* out, size_t outMax) {
    if (out == nullptr || outMax == 0) {
        return 0;
    }
    static const char digits[] = "0123456789abcdef";
    size_t n = 0;
    for (size_t i = 0; i < len && n + 2 < outMax; i++) {
        out[n++] = digits[data[i] >> 4];
        out[n++] = digits[data[i] & 0x0F];
    }
    out[n] = '\0';
    return n;
}

class AntDiagBuffer {
public:
    void begin(uint32_t existingFileBytes) {
        fileBytes = existingFileBytes;
        committed = 0;
        pendingLen = 0;
        droppedLines = 0;
        isFull = existingFileBytes >= (uint32_t)(ANT_DIAG_FILE_CAP - ANT_DIAG_TRUNCATED_LEN);
    }

    // Records the size of a file that already exists without discarding the
    // lines queued so far (the driver queues lines before LittleFS is read).
    // Lines were accepted against a file size of 0, so the queue is re-checked
    // against the real size: a file already at the cap drops them all (it
    // already ends in "# truncated"); otherwise only whole lines that fit are
    // kept, followed by the truncated mark.
    void setExistingFileBytes(uint32_t existingFileBytes) {
        fileBytes = existingFileBytes;
        const uint32_t limit = (uint32_t)(ANT_DIAG_FILE_CAP - ANT_DIAG_TRUNCATED_LEN);
        if (existingFileBytes >= limit) {
            pendingLen = 0;
            isFull = true;
            return;
        }
        if (fileBytes + committed + (uint32_t)pendingLen <= limit) {
            return;
        }
        const uint32_t room = limit - fileBytes - committed;
        size_t keep = 0;
        for (size_t i = 0; i < pendingLen && i < room; i++) {
            if (pending[i] == '\n') keep = i + 1;
        }
        pendingLen = keep;
        if (!isFull) {
            memcpy(pending + pendingLen, ANT_DIAG_TRUNCATED_MARK, ANT_DIAG_TRUNCATED_LEN);
            pendingLen += ANT_DIAG_TRUNCATED_LEN;
            isFull = true;
        }
    }

    // Stores "<ms> <text>\n". Returns false when the line was rejected: the
    // file is full (nothing counted) or the pending RAM is out of room
    // (counted, and reported before the next accepted line).
    bool addLine(uint32_t ms, const char* text) {
        if (isFull) {
            return false;
        }
        char marker[40];
        size_t markerLen = 0;
        if (droppedLines > 0) {
            int m = snprintf(marker, sizeof(marker), "%lu # dropped %lu lines\n",
                             (unsigned long)ms, (unsigned long)droppedLines);
            markerLen = (m > 0 && (size_t)m < sizeof(marker)) ? (size_t)m : 0;
        }
        char line[ANT_DIAG_LINE_MAX];
        int l = snprintf(line, sizeof(line), "%lu %s\n", (unsigned long)ms, text ? text : "");
        size_t lineLen;
        if (l < 0) {
            return false;
        } else if ((size_t)l >= sizeof(line)) {
            lineLen = sizeof(line) - 1;
            line[lineLen - 1] = '\n';
        } else {
            lineLen = (size_t)l;
        }

        size_t need = markerLen + lineLen;
        if (pendingLen + need > ANT_DIAG_PENDING_CAP) {
            droppedLines++;
            return false;
        }
        uint32_t projected = fileBytes + committed + (uint32_t)pendingLen;
        if (projected + need > (uint32_t)(ANT_DIAG_FILE_CAP - ANT_DIAG_TRUNCATED_LEN)) {
            // The pending array has room for the mark beyond ANT_DIAG_PENDING_CAP.
            memcpy(pending + pendingLen, ANT_DIAG_TRUNCATED_MARK, ANT_DIAG_TRUNCATED_LEN);
            pendingLen += ANT_DIAG_TRUNCATED_LEN;
            isFull = true;
            return false;
        }
        if (markerLen > 0) {
            memcpy(pending + pendingLen, marker, markerLen);
            pendingLen += markerLen;
            droppedLines = 0;
        }
        memcpy(pending + pendingLen, line, lineLen);
        pendingLen += lineLen;
        return true;
    }

    // Copies up to outMax pending bytes without consuming them.
    size_t takePending(char* out, size_t outMax) const {
        size_t n = pendingLen < outMax ? pendingLen : outMax;
        if (n > 0) {
            memcpy(out, pending, n);
        }
        return n;
    }

    // Drops the first n pending bytes (they were written to flash).
    void commitWritten(size_t n) {
        if (n > pendingLen) {
            n = pendingLen;
        }
        memmove(pending, pending + n, pendingLen - n);
        pendingLen -= n;
        committed += (uint32_t)n;
    }

    bool full() const { return isFull; }
    uint32_t dropped() const { return droppedLines; }

private:
    char pending[ANT_DIAG_PENDING_CAP + ANT_DIAG_TRUNCATED_LEN];
    size_t pendingLen = 0;
    uint32_t fileBytes = 0;
    uint32_t committed = 0;
    uint32_t droppedLines = 0;
    bool isFull = false;
};
