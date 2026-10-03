#pragma once
#include <stdint.h>
#include "PowerAlertLogic.h"

class PowerAlert {
public:
    PowerAlert();
    void handle();

    uint8_t  getActiveCauses() const { return activeCauses_; }

private:
    PowerAlertLogic logic_;
    uint8_t  activeCauses_;
};
