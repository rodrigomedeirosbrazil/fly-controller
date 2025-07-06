#include "Power.h"
#include "../config.h"

Power::Power() {}

unsigned int Power::limit(unsigned int throttlePercentage)
{
    #if ENABLED_POWER_LIMIT
        if (throttlePercentage > POWER_LIMIT_PERCENTAGE) {
            return POWER_LIMIT_PERCENTAGE;
        }
    #endif
    return throttlePercentage;
}
