#include <Arduino.h>

#include <Servo.h>
#include <SPI.h>
#include <mcp2515.h>

#include "config.h"
#include "main.h"
#include "PWMread_RCfailsafe.h"

#include "Throttle/Throttle.h"
#include "PwmReader/PwmReader.h"
#include "Display/Display.h"
#include "Screen/Screen.h"
#include "Canbus/Canbus.h"

Servo esc;
PwmReader pwmReader;
Throttle throttle(&pwmReader);
Display display;
Screen screen(&display, &throttle);
Canbus canbus;

MCP2515 mcp2515(CANBUS_CS_PIN);
struct can_frame canMsg;

unsigned long lastRcUpdate;

void setup()
{
  #if SERIAL_DEBUG
    Serial.begin(9600);
  #endif

  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  #if SERIAL_DEBUG
    Serial.println("------- CAN Read ----------");
    Serial.println("ID  DLC   DATA");
  #endif

  display.begin();
  display.setFlipMode(1);

  setup_pwmRead();
  esc.attach(ESC_PIN);
}

void loop()
{
  unsigned long now = millis();

  screen.draw();
  checkCanbus();

  if (RC_avail() || now - lastRcUpdate > PWM_FRAME_TIME_MS)
  {
    lastRcUpdate = now;

    pwmReader.tick(RC_decode(THROTTLE_CHANNEL) * 100);

    throttle.tick();

    handleEsc();
  }
}

void handleEsc()
{
  int pulseWidth = ESC_MIN;

  if (!throttle.isArmed())
  {
    esc.writeMicroseconds(pulseWidth);
    return;
  }

  pulseWidth = map(
    throttle.isCruising() 
      ? throttle.getCruisingThrottlePosition()
      : throttle.getThrottlePercentageFiltered(),
    0, 
    100, 
    ESC_MIN, 
    ESC_MAX
  );

    esc.writeMicroseconds(
      throttle.isArmed()
      ? pulseWidth
      : 0
    );

    return;
}

void checkCanbus() 
{
    if (mcp2515.readMessage(&canMsg) == MCP2515::ERROR_OK) {
        #if SERIAL_DEBUG 
        Serial.print(millis());
        Serial.print(" "); 
        Serial.print(canMsg.can_id, HEX); // print ID
        Serial.print(" "); 
        Serial.print(canMsg.can_dlc, HEX); // print DLC
        Serial.print(" ");
        
        for (int i = 0; i<canMsg.can_dlc; i++)  {  // print the data
            Serial.print(canMsg.data[i],HEX);
            Serial.print(" ");
        }

        Serial.println();
        #endif

        canbus.parseCanMsg(&canMsg);
    }
}