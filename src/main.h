#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <AceButton.h>

#include "WebServer/ControllerWebServer.h"

using namespace ace_button;

void handleEsc();
void checkCanbus();
void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState);
bool isMotorRunning();
void handleArmedBeep();

extern ControllerWebServer webServer;

#endif
