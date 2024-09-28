#include "PwmReader.h"

PwmReader::PwmReader()
{
    initRawReadings();
    throttlePercentage = 0;
}

void PwmReader::tick(int rawThrottlePercentage)
{
    throttlePercentage = getAverageReading(rawThrottlePercentage);
}

void PwmReader::initRawReadings()
{
    for (int i = 0; i < NUM_READINGS; i++)
    {
        rawReadings[i] = 0;
    }
}

int PwmReader::getAverageReading(int newRead)
{
    for (int i = 1; i < NUM_READINGS; i++)
    {
        rawReadings[i - 1] = rawReadings[i];
    }
    rawReadings[NUM_READINGS - 1] = newRead;

    int total = 0;
    for (int i = 0; i < NUM_READINGS; i++)
    {
        total += rawReadings[i];
    }

    return total / NUM_READINGS;
}