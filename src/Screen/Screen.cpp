#include <Arduino.h>
#include "Screen.h"

Screen::Screen(
    Display *display,
    Throttle *throttle,
    Canbus *canbus,
    Temperature *motorTemp
) {
    this->display = display;
    this->throttle = throttle;
    this->canbus = canbus;
    this->motorTemp = motorTemp;
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
    drawCurrent();
    drawMotorTemp();
    drawThrottleBar();
    drawEscInfo();
    drawRpm();
    drawArmed();
    drawCruise();

    this->display->sendBuffer();
}

void Screen::drawMotorTemp() {
    this->display->drawXBMP(0, 17, 11, 16, image_refresh_bits);
    this->display->setCursor(15, 30);

    char buffer[3];
    dtostrf(motorTemp->getTemperature(), 2, 0, buffer);
    this->display->print(buffer);
    this->display->print("C");
}

void Screen::drawCurrent() {
    this->display->setCursor(2, 12);

    if (! canbus->isReady()) {
        this->display->print("---.-A");
        return;
    }

    char buffer[6];
    dtostrf(canbus->getMiliCurrent() / 10.0, 3, 1, buffer);
    this->display->print(buffer);
    this->display->print("A");
}

void Screen::drawEscInfo() {
    this->display->drawStr(2, 46, "ESC");
    this->display->setCursor(25, 46);

    if (! canbus->isReady()) {
        this->display->print("--C");
        return;
    }

    char buffer[5];
    sprintf(buffer, "%3dC", canbus->getTemperature());
    this->display->print(buffer);
}

void Screen::drawRpm() {
    this->display->drawStr(72, 46, "RPM");
    this->display->setCursor(100, 46);

    if (! canbus->isReady()) {
        this->display->print("----");
        return;
    }

    char buffer[5];
    sprintf(buffer, "%4d", canbus->getRpm());
    this->display->print(buffer);
}

void Screen::drawThrottleBar() {
    unsigned int throttlePercentage = this->throttle->getThrottlePercentage();

    this->display->setCursor(70, 29);

    char buffer[5];
    sprintf(buffer, "%3d%%", throttlePercentage);
    this->display->print(buffer);

    this->display->setDrawColor(2);

    int throttleBarWidth = map(throttlePercentage, 0, 100, 0, 80);
    this->display->drawBox(42, 19, throttleBarWidth, 11);
    this->display->drawFrame(40, 17, 84, 15); // throttle frame

    this->display->setDrawColor(1);
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
    if (! canbus->isReady()) {
        return;
    }

    int batteryMilliVolts = canbus->getMiliVoltage();
    int batteryPercentage = map(
        batteryMilliVolts, 
        BATTERY_MIN_VOLTAGE, 
        BATTERY_MAX_VOLTAGE,
        0, 
        100
    );

    char buffer[7];
    sprintf(buffer, "%3d%%", batteryPercentage);
    this->display->setCursor(49, 12);
    this->display->print(buffer);

    dtostrf(batteryMilliVolts / 10.0, 2, 1, buffer);
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