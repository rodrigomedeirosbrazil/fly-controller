#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>

#include "WebServer/ControllerWebServer.h"
#include "Logger/Logger.h"

void handleEsc();
void checkCanbus();
bool isMotorRunning();
void updateSoundState();

extern ControllerWebServer webServer;
extern Logger logger;

#endif
