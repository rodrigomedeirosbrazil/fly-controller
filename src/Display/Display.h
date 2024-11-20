#ifndef DISPLAY_H
#define DISPLAY_H

#include <U8g2lib.h>
#include <Wire.h>
#include "../config.h"

class Display : public U8G2_SH1106_128X64_NONAME_1_HW_I2C
{
public:
    Display() : U8G2_SH1106_128X64_NONAME_1_HW_I2C(U8G2_R0, U8X8_PIN_NONE) {
        Wire.setWireTimeout(5000, true);
    };
};

#endif