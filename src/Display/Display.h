#ifndef DISPLAY_H
#define DISPLAY_H

#include <U8g2lib.h>
#include <Wire.h>
#include "../config.h"

class Display : public U8G2_SH1106_128X64_NONAME_1_HW_I2C
{
public:
    Display() : U8G2_SH1106_128X64_NONAME_1_HW_I2C(U8G2_R0, U8X8_PIN_NONE) {};

    void printCenter(char *text, uint8_t textX, uint8_t textY);
    void printCenter(int value, uint8_t textX, uint8_t textY);
    void printCenter(float value, uint8_t textX, uint8_t textY);

    void printRight(char *text, uint8_t textX, uint8_t textY);
    void printRight(int value, uint8_t textX, uint8_t textY);
    void printRight(float value, uint8_t textX, uint8_t textY);
};

#endif