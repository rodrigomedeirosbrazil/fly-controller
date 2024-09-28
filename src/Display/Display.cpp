#include "Display.h"

void Display::printCenter(char *text, uint8_t textX, uint8_t textY)
{
    unsigned int chars = strlen(text);
    unsigned char x = textX - ((chars * getMaxCharWidth()) / 2);

    drawStr(x, textY, text);
}

void Display::printCenter(int value, uint8_t textX, uint8_t textY)
{
    char buffer[6];
    sprintf(buffer, "%d", value);
    printCenter(buffer, textX, textY);
}

void Display::printCenter(float value, uint8_t textX, uint8_t textY)
{
    char buffer[6]; 
    dtostrf(value, 2, 2, buffer);
    printCenter(buffer, textX, textY);
}

void Display::printRight(char *text, uint8_t textX, uint8_t textY)
{
    unsigned int chars = strlen(text);
    unsigned char x = textX - (chars * getMaxCharWidth());

    drawStr(x, textY, text);
}

void Display::printRight(int value, uint8_t textX, uint8_t textY)
{
    char buffer[6];
    sprintf(buffer, "%d", value);
    printRight(buffer, textX, textY);
}

void Display::printRight(float value, uint8_t textX, uint8_t textY)
{
    char buffer[6]; 
    dtostrf(value, 2, 2, buffer);
    printRight(buffer, textX, textY);
}