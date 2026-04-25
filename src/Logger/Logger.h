#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

class Logger {
public:
    Logger();
    void init();
    void startLogging();
    void log(const char* data);
    void log(const String &data);
    void setHeader(const String &header);
    /** Call after log files were removed from LittleFS (e.g. web UI delete-all). */
    void afterLogFilesClearedFromStorage();
    /**
     * Flush and release the file handle so another reader can open the same file
     * without seeing a stale cached size. The file is reopened automatically on
     * the next log() call.
     */
    void closeLogFile();
    ~Logger();

    /** Returns true if the system clock has been set (epoch > 2020). */
    static bool isTimeSynced();

private:
    String currentFileName;
    File logFile;
    bool fileOpen;
    bool loggingEnabled;
    bool wasArmed;
    String csvHeader;

    void createNewFile();
    void openLogFile();
    void stopLogging();

    /** Writes current time as YYYY-MM-DDTHH:MM:SS into buf, or "ms:NNNNNNN" if not synced. */
    static void formatTimestamp(char* buf, size_t len);
};

#endif
