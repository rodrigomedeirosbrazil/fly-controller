#include "LogStore.h"
#include <LittleFS.h>
#include <vector>

namespace {

// "/<name>" into a stack buffer. False for anything longer than a log name,
// which also keeps a hostile length from overrunning `out`.
bool toPath(const char* name, size_t len, char (&out)[LOG_MAX_NAME_LEN + 2]) {
    if (name == nullptr || len == 0 || len > LOG_MAX_NAME_LEN) {
        return false;
    }
    out[0] = '/';
    memcpy(out + 1, name, len);
    out[len + 1] = '\0';
    return true;
}

// Calls fn(name, len, size) for every regular file in the root, with any
// leading '/' stripped (Arduino-ESP32 versions disagree on whether name()
// carries it).
template <typename Fn>
void forEachRootFile(Fn fn) {
    File root = LittleFS.open("/");
    if (!root) {
        return;
    }
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            if (name[0] == '/') {
                name++;
            }
            fn(name, strlen(name), (uint32_t) file.size());
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

} // namespace

namespace LogStore {

uint32_t usedBytes()  { return (uint32_t) LittleFS.usedBytes(); }
uint32_t totalBytes() { return (uint32_t) LittleFS.totalBytes(); }

void list(LogPageBuilder& builder) {
    forEachRootFile([&](const char* name, size_t len, uint32_t size) {
        builder.offer(name, len, size);
    });
}

bool fileSize(const char* name, size_t len, uint32_t& size) {
    char path[LOG_MAX_NAME_LEN + 2];
    if (!toPath(name, len, path) || !LittleFS.exists(path)) {
        return false;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        return false;
    }
    size = (uint32_t) f.size();
    f.close();
    return true;
}

size_t read(const char* name, size_t nameLen, uint32_t offset, uint8_t* buf, size_t len) {
    char path[LOG_MAX_NAME_LEN + 2];
    if (len == 0 || !toPath(name, nameLen, path) || !LittleFS.exists(path)) {
        return 0;
    }
    File f = LittleFS.open(path, "r");
    if (!f) {
        return 0;
    }
    size_t n = 0;
    if (f.seek(offset)) {
        n = f.read(buf, len);
    }
    f.close();
    return n;
}

bool remove(const char* name, size_t len) {
    char path[LOG_MAX_NAME_LEN + 2];
    if (!toPath(name, len, path) || !LittleFS.exists(path)) {
        return false;
    }
    return LittleFS.remove(path);
}

uint16_t removeAll() {
    // Collected first: removing while the directory is open for iteration is
    // not safe on LittleFS.
    std::vector<String> names;
    forEachRootFile([&](const char* name, size_t len, uint32_t) {
        if (hasLogExtension(name, len)) {
            names.push_back(String("/") + name);
        }
    });
    uint16_t removed = 0;
    for (const String& path : names) {
        if (LittleFS.remove(path)) {
            removed++;
        }
    }
    return removed;
}

uint8_t rotate(const char* current, size_t currentLen,
               uint32_t reserveBytes, uint8_t maxDeletions) {
    uint8_t deleted = 0;
    while (deleted < maxDeletions &&
           logFreeBytes(totalBytes(), usedBytes()) < reserveBytes) {
        LogRetentionPicker picker;
        picker.reset(current, currentLen);
        forEachRootFile([&](const char* name, size_t len, uint32_t size) {
            picker.offer(name, len, size);
        });
        if (!picker.hasPick() || !remove(picker.pick(), picker.pickLen())) {
            break;
        }
        Serial.printf("[LogStore] retention removed %s\n", picker.pick());
        deleted++;
    }
    return deleted;
}

} // namespace LogStore
