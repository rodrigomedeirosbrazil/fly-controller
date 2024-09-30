#include <Arduino.h>
#include "PwmReader.h"

PwmReader::PwmReader()
{
    throttlePercentage = 0;
}

void PwmReader::tick(int rawThrottlePercentage)
{
    throttlePercentage = rawThrottlePercentage;
}
