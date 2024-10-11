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

    drawBatteryBar();
    
    this->display->drawStr(15, 12, "--C");
    this->display->drawStr(15, 30, "--C");

    drawThrottleBar();
        
    this->display->drawXBMP(-1, 0, 16, 16, image_voltage_bits);
    this->display->drawLine(0, 0, 0, 0);
    this->display->setDrawColor(2);

    this->display->setDrawColor(1);

    this->display->drawStr(72, 46, "RPM");
    this->display->drawXBMP(0, 17, 11, 16, image_refresh_bits);
    this->display->drawStr(2, 46, "ESC");
    this->display->drawStr(25, 46, "--C");

    drawArmed();
    drawCruise();

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
    this->display->drawFrame(40, 17, 84, 15); // throttle frame
}

void Screen::drawArmed() {
    if (!this->throttle->isArmed()) {
        this->display->drawStr(9, 61, "DISARMED");
        return;
    }

    this->display->drawBox(1, 50, 64, 13);
    this->display->setDrawColor(2);
    this->display->drawStr(18, 61, "ARMED");
    this->display->setDrawColor(1);
}

void Screen::drawCruise() {
    if (! this->throttle->isCruising()) {
        return;
    }

    this->display->drawBox(64, 50, 62, 13);
    this->display->setDrawColor(2);
    this->display->drawStr(78, 61, "CRUISE");
    this->display->setDrawColor(1);
}

void Screen::drawBatteryBar() {
    int batteryMilliVolts = 5634;
    int batteryPercentage = map(batteryMilliVolts, 4760, 5880, 0, 100);

    char buffer[7];
    sprintf(buffer, "%3d%%", batteryPercentage);
    this->display->setCursor(49, 12);
    this->display->print(buffer);

    dtostrf(batteryMilliVolts / 100.0, 2, 1, buffer);
    this->display->setCursor(90, 12);
    this->display->print(buffer);
    this->display->print("V");

    this->display->drawFrame(40, 0, 84, 15); // battery frame

    this->display->setDrawColor(2);

    int batteryBarWidth = map(batteryPercentage, 0, 100, 0, 80);
    this->display->drawBox(42, 2, batteryBarWidth, 11); // battery bar
    this->display->drawBox(123, 3, 3, 9); // battery head

    this->display->setDrawColor(1);
}