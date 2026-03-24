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
    /** Call after log .txt files were removed from LittleFS (e.g. web UI delete-all). */
    void afterLogFilesClearedFromStorage();
    ~Logger();

private:
    String currentFileName;
    File logFile;
    bool fileOpen;
    bool loggingEnabled;
    bool wasArmed;
    String csvHeader;

    void createNewFile();
    void openLogFile();
    void closeLogFile();
    void stopLogging();
};

#endif
