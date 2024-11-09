#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <AceButton.h>

using namespace ace_button;

void handleEsc();
void handleSerialLog();
void checkCanbus();
unsigned int analizeTelemetryToThrottleOutput(unsigned int throttlePercentage);
void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState);

#endif
