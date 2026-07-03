#include "Logger.h"
#include "../Throttle/Throttle.h"
#include <time.h>
#include <sys/time.h>

extern Throttle throttle;

static const time_t MIN_VALID_EPOCH = 1577836800; // 2020-01-01 UTC

bool Logger::isTimeSynced() {
    return time(nullptr) > MIN_VALID_EPOCH;
}

void Logger::formatTimestamp(char* buf, size_t len) {
    if (!isTimeSynced()) {
        snprintf(buf, len, "ms:%lu", millis());
        return;
    }

    time_t now = time(nullptr);
    struct tm t;
    gmtime_r(&now, &t);

    if (fileHasDate) {
        snprintf(buf, len, "%02d:%02d:%02d",
                 t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                 t.tm_hour, t.tm_min, t.tm_sec);
    }
}

Logger::Logger() {
    currentFileName = "";
    fileOpen = false;
    loggingEnabled = false;
    wasArmed = false;
    fileHasDate = false;
    csvHeader = "";
}

void Logger::init() {
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
        return;
    }
    // Don't create/open log file yet - wait for startLogging()
}

void Logger::startLogging() {
    if (loggingEnabled) {
        return; // Already logging
    }

    loggingEnabled = true;
    createNewFile();
    openLogFile();
}

Logger::~Logger() {
    closeLogFile();
}

void Logger::createNewFile() {
    // Build date prefix when clock is synced (e.g. "20250419"), else empty.
    char datePrefix[12] = "";
    if (isTimeSynced()) {
        time_t now = time(nullptr);
        struct tm t;
        gmtime_r(&now, &t);
        snprintf(datePrefix, sizeof(datePrefix), "%04d%02d%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    }
    const bool useDate = (datePrefix[0] != '\0');
    fileHasDate = useDate;

    File root = LittleFS.open("/");
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }

    root.rewindDirectory();

    int maxSeqNum = 0;       // highest NNN seen for the date prefix (or overall)
    int minEmptySeqNum = -1; // lowest NNN of an empty file for this prefix

    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        if (fileName.startsWith("/")) fileName = fileName.substring(1);

        // Accept both YYYYMMDD_NNN.csv (date) and NNNNN.csv (legacy).
        String seqPart;
        if (useDate) {
            String expectedPrefix = String(datePrefix) + "_";
            if (fileName.startsWith(expectedPrefix) && fileName.endsWith(".csv")) {
                seqPart = fileName.substring(expectedPrefix.length(),
                                             fileName.length() - 4);
            }
        } else {
            int dotIndex = fileName.indexOf('.');
            if (dotIndex > 0) {
                seqPart = fileName.substring(0, dotIndex);
            }
        }

        if (seqPart.length() > 0) {
            bool allDigits = true;
            for (unsigned int i = 0; i < seqPart.length(); i++) {
                if (!isdigit(seqPart.charAt(i))) { allDigits = false; break; }
            }
            if (allDigits) {
                int num = seqPart.toInt();
                if (num > maxSeqNum) maxSeqNum = num;
                if (file.size() == 0 && (minEmptySeqNum < 0 || num < minEmptySeqNum)) {
                    minEmptySeqNum = num;
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    char fileNameBuf[32];

    if (minEmptySeqNum >= 0) {
        if (useDate) {
            snprintf(fileNameBuf, sizeof(fileNameBuf), "/%s_%03d.csv", datePrefix, minEmptySeqNum);
        } else {
            snprintf(fileNameBuf, sizeof(fileNameBuf), "/%05d.csv", minEmptySeqNum);
        }
        currentFileName = String(fileNameBuf);
        Serial.print("Reusing empty log file: ");
        Serial.println(currentFileName);
        return;
    }

    int nextSeqNum = maxSeqNum + 1;
    if (useDate) {
        snprintf(fileNameBuf, sizeof(fileNameBuf), "/%s_%03d.csv", datePrefix, nextSeqNum);
    } else {
        snprintf(fileNameBuf, sizeof(fileNameBuf), "/%05d.csv", nextSeqNum);
    }
    currentFileName = String(fileNameBuf);

    // POWER LOSS SAFETY
    File newFile = LittleFS.open(currentFileName, "w");
    if (newFile) {
        newFile.close();
        Serial.print("New log file created: ");
        Serial.println(currentFileName);
    } else {
        Serial.print("Failed to create log file: ");
        Serial.println(currentFileName);
        currentFileName = "";
    }
}

void Logger::openLogFile() {
    if (currentFileName.length() == 0) return;

    if (fileOpen) {
        closeLogFile();
    }

    logFile = LittleFS.open(currentFileName, "a");
    if (!logFile) {
        Serial.println("Failed to open log file for appending");
        fileOpen = false;
        return;
    }
    fileOpen = true;

    if (logFile.size() == 0 && csvHeader.length() > 0) {
        logFile.print("timestamp,");
        logFile.print(csvHeader);
        logFile.print("\r\n");
        logFile.flush();
    }
}

void Logger::closeLogFile() {
    if (fileOpen && logFile) {
        logFile.flush();
        logFile.close();
        fileOpen = false;
    }
}

void Logger::stopLogging() {
    if (loggingEnabled) {
        closeLogFile();
        loggingEnabled = false;
        currentFileName = "";
    }
}

void Logger::setHeader(const String &header) {
    csvHeader = header;
}

void Logger::afterLogFilesClearedFromStorage() {
    closeLogFile();
    createNewFile();
    if (loggingEnabled) {
        openLogFile();
    }
}

void Logger::log(const char* data) {
    bool isArmed = throttle.isArmed();

    // Detect transition from armed to disarmed
    if (wasArmed && !isArmed) {
        stopLogging();
    }

    // Start logging automatically once throttle is armed
    if (!loggingEnabled && isArmed) {
        startLogging();
    }

    // Only log when throttle is armed
    if (!isArmed) {
        wasArmed = false;
        return; // Don't log when throttle is not armed
    }

    wasArmed = true;

    if (!loggingEnabled) {
        return; // Don't log until logging is enabled
    }

    if (currentFileName.length() == 0) return;

    // Ensure file is open
    if (!fileOpen) {
        openLogFile();
        if (!fileOpen) {
            Serial.println("Failed to open log file");
            return;
        }
    }

    char tsBuf[24];
    formatTimestamp(tsBuf, sizeof(tsBuf));
    logFile.print(tsBuf);
    logFile.print(",");
    logFile.print(data);
    logFile.flush(); // Force data to be written to storage immediately
}

void Logger::log(const String &data) {
    log(data.c_str());
}
