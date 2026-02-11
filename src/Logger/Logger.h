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
    void log(const String &data);
    void setHeader(const String &header);
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
