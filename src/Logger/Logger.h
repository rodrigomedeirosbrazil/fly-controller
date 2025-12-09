#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>

class Logger {
public:
    Logger();
    void init();
    void log(const String &data);
    ~Logger();

private:
    String currentFileName;
    File logFile;
    bool fileOpen;

    void createNewFile();
    void openLogFile();
    void closeLogFile();
    void checkSpaceAndRotate();
};

#endif
