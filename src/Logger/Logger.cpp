#include "Logger.h"
#include "../Throttle/Throttle.h"

#define MIN_FREE_SPACE_BYTES 102400 // 100KB

extern Throttle throttle;

Logger::Logger() {
    currentFileName = "";
    fileOpen = false;
    loggingEnabled = false;
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
        file = root.openNextFile();
    }

    int nextFileNum = maxFileNum + 1;
    char fileNameBuf[16];
    snprintf(fileNameBuf, sizeof(fileNameBuf), "/%05d.txt", nextFileNum);
    currentFileName = String(fileNameBuf);

    Serial.print("New log file created: ");
    Serial.println(currentFileName);
}

void Logger::openLogFile() {
    if (currentFileName.length() == 0) return;

    if (fileOpen) {
        closeLogFile();
    }

    logFile = LittleFS.open(currentFileName, "a"); // Append mode
    if (!logFile) {
        Serial.println("Failed to open log file for appending");
        fileOpen = false;
        return;
    }
    fileOpen = true;
}

void Logger::closeLogFile() {
    if (fileOpen && logFile) {
        logFile.flush(); // Ensure all data is written before closing
        logFile.close();
        fileOpen = false;
    }
}

void Logger::log(const String &data) {
    // Start logging automatically once throttle calibration is complete
    if (!loggingEnabled && throttle.isCalibrated()) {
        startLogging();
    }

    if (!loggingEnabled) {
        return; // Don't log until logging is enabled
    }

    if (currentFileName.length() == 0) return;

    checkSpaceAndRotate();

    // Ensure file is open
    if (!fileOpen) {
        openLogFile();
        if (!fileOpen) {
            Serial.println("Failed to open log file");
            return;
        }
    }

    // Write data and immediately flush to ensure it's written to storage
    logFile.print(data);
    logFile.flush(); // Force data to be written to storage immediately
}

void Logger::checkSpaceAndRotate() {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    size_t free = total - used;

    while (free < MIN_FREE_SPACE_BYTES) {
        Serial.printf("Low space: %u bytes. Rotating...\n", free);

        // Close current file before rotating
        closeLogFile();

        // Find oldest file
        File root = LittleFS.open("/");
        int minFileNum = -1;
        String oldestFileName = "";

        File file = root.openNextFile();
        while (file) {
            String fileName = file.name();
             if (fileName.startsWith("/")) {
                fileName = fileName.substring(1);
            }

            // Only rotate our log files
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
                    // Don't delete current file
                    String currentNumPart = currentFileName;
                     if (currentNumPart.startsWith("/")) currentNumPart = currentNumPart.substring(1);
                     currentNumPart = currentNumPart.substring(0, currentNumPart.indexOf('.'));

                    if (num != currentNumPart.toInt()) {
                        if (minFileNum == -1 || num < minFileNum) {
                            minFileNum = num;
                            oldestFileName = file.name(); // Keep original name with slash if needed by FS
                        }
                    }
                }
            }
            file = root.openNextFile();
        }

        if (minFileNum != -1 && oldestFileName.length() > 0) {
            // Ensure path has slash
             String pathToDelete = oldestFileName;
             if (!pathToDelete.startsWith("/")) pathToDelete = "/" + pathToDelete;

            Serial.print("Deleting old log: ");
            Serial.println(pathToDelete);
            LittleFS.remove(pathToDelete);

            // Update free space
            used = LittleFS.usedBytes();
            free = total - used;
        } else {
            Serial.println("No deletable log files found or disk full with current file.");
            break;
        }

        // Reopen file after rotation
        openLogFile();
    }
}
