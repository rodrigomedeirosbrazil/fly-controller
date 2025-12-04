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

private:
    String currentFileName;
    
    void createNewFile();
    void checkSpaceAndRotate();
};

#endif
