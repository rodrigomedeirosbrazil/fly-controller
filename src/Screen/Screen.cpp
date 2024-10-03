#include <Arduino.h>
#include "Screen.h"

Screen::Screen(
    Display *display,
    Throttle *throttle
) {
    this->display = display;
    this->throttle = throttle;
}

void Screen::draw()
{
  if (millis() - lastScreenUpdate < 250)
  {
    return;
  }

  lastScreenUpdate = millis();

  this->display->firstPage();
    do {
      drawUi();
    } while (this->display->nextPage());
}

void Screen::drawUi() {
    this->display->clearBuffer();
    this->display->setFontMode(1);
    this->display->setBitmapMode(1);
    this->display->setFont(u8g2_font_6x13_tr);
    this->display->drawStr(49, 12, "---%");
    this->display->drawFrame(40, 0, 84, 15);
    this->display->setDrawColor(2);
    this->display->drawFrame(123, 3, 3, 9);
    this->display->setDrawColor(1);
    this->display->drawStr(97, 12, "--V");
    this->display->drawStr(15, 12, "--C");
    this->display->drawStr(15, 30, "--C");
    this->display->drawFrame(40, 17, 84, 15);

    drawThrottleBar();
        
    this->display->drawXBMP(-1, 0, 16, 16, image_voltage_bits);
    this->display->drawLine(0, 0, 0, 0);
    this->display->setDrawColor(2);
    this->display->drawBox(42, 2, 80, 11);

    this->display->setDrawColor(1);

    this->display->drawStr(72, 46, "RPM");
    this->display->drawXBMP(0, 17, 11, 16, image_refresh_bits);
    this->display->drawStr(2, 46, "ESC");
    this->display->drawStr(25, 46, "--C");

    if (this->throttle->isArmed()) {
        this->display->drawStr(10, 61, "ARMED");
    } else {
        this->display->drawStr(10, 61, "DISARMED");
    }

    if (this->throttle->isCruising()) {
        this->display->drawStr(78, 61, "CRUISE");
    }

    this->display->drawStr(100, 46, "----");
    this->display->sendBuffer();
}

void Screen::drawThrottleBar() {
    unsigned int throttlePercentage = this->throttle->getThrottlePercentageFiltered();

    this->display->setDrawColor(1);
    this->display->setCursor(70, 29);

    char buffer[5];
    sprintf(buffer, "%3d%%", throttlePercentage);
    this->display->print(buffer);

    this->display->setDrawColor(2);

    int throttleBarWidth = map(throttlePercentage, 0, 100, 0, 80);
    this->display->drawBox(42, 19, throttleBarWidth, 11);
}