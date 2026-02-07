#include "Logger.h"
#include "../Throttle/Throttle.h"

extern Throttle throttle;

Logger::Logger() {
    currentFileName = "";
    fileOpen = false;
    loggingEnabled = false;
    wasArmed = false;
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
    File root = LittleFS.open("/");
    if (!root) {
        Serial.println("Failed to open directory");
        return;
    }

    root.rewindDirectory();

    int maxFileNum = 0;

    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        // Assuming format /00001.txt - remove leading / if present
        if (fileName.startsWith("/")) {
            fileName = fileName.substring(1);
        }

        // Check if it matches pattern digit+.txt
        int dotIndex = fileName.indexOf('.');
        if (dotIndex > 0) {
            String numPart = fileName.substring(0, dotIndex);
            bool isDigit = true;
            for (unsigned int i = 0; i < numPart.length(); i++) {
                if (!isdigit(numPart.charAt(i))) {
                    isDigit = false;
                    break;
                }
            }
            if (isDigit) {
                int num = numPart.toInt();
                if (num > maxFileNum) {
                    maxFileNum = num;
                }
            }
        }
        file.close();
        file = root.openNextFile();
    }

    root.close();

    int nextFileNum = maxFileNum + 1;
    char fileNameBuf[16];
    snprintf(fileNameBuf, sizeof(fileNameBuf), "/%05d.txt", nextFileNum);
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
        currentFileName = ""; // Limpar nome se falhou
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
        // Prepare new file for next session
        createNewFile();
    }
}

void Logger::log(const String &data) {
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

    logFile.print(data);
    logFile.flush(); // Force data to be written to storage immediately
}
