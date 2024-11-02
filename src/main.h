#ifndef MAIN_H
#define MAIN_H

void handleEsc();
void handleSerialLog();
void checkCanbus();
unsigned int analizeTelemetryToThrottleOutput(unsigned int throttlePercentage);

#endif
