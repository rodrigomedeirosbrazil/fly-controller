#include <Arduino.h>

#include "defines.h"
#include "Display/Display.h"

Display display;

unsigned long lastDisplayUpdate = 0;

void setup() {
  display.begin();
}

void loop() {
  if (millis() - lastDisplayUpdate < 1000)
  {
    return;
  }

  lastDisplayUpdate = millis();

  display.firstPage();
    do {
      display.setFont(BIG_FONT);

      display.setCursor(0, 12);
      display.print("87% 58.8V 43C");

      display.setCursor(0, 12 * 2);
      display.print("25% 47.5A 2726W");

      display.setCursor(0, 12 * 3);
      display.print("1200 RPM 56C");

      display.setCursor(0, 12 * 4);
      display.print("ESC 56C");

    } while (display.nextPage());
}
