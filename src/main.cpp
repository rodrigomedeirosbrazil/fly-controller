#include <Arduino.h>

#include <ESP32Servo.h>
#include <driver/twai.h>
#include <AceButton.h>

#include "config.h"
#include "main.h"

#include "Throttle/Throttle.h"

#include "Canbus/Canbus.h"
#include "Hobbywing/Hobbywing.h"
#include "Temperature/Temperature.h"
#include "Buzzer/Buzzer.h"
#include "Power/Power.h"
#include "Xctod/Xctod.h"

using namespace ace_button;
#include "Button/Button.h"

void setup()
{
  // Configure ADC attenuation for full 0-3.3V range
  analogSetAttenuation(ADC_ATTENUATION);
  
  xctod.init();
  buzzer.setup();
  
  // Initialize TWAI (CAN) driver with SN65HVD230
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)CAN_TX_PIN, 
    (gpio_num_t)CAN_RX_PIN, 
    TWAI_MODE_NORMAL
  );
  twai_timing_config_t t_config = CAN_BITRATE;
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("TWAI driver installed");
  } else {
    Serial.println("Failed to install TWAI driver");
  }
  
  if (twai_start() == ESP_OK) {
    Serial.println("TWAI driver started");
  } else {
    Serial.println("Failed to start TWAI driver");
  }

  hobbywing.announce();
  hobbywing.requestEscId();
  hobbywing.setThrottleSource(Hobbywing::throttleSourcePWM);
  hobbywing.setLedColor(Hobbywing::ledColorGreen);

  buzzer.beepWarning();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_MIN_PWM);
}

void loop()
{
  button.check();
  xctod.write();
  checkCanbus();
  throttle.handle();
  motorTemp.handle();
  buzzer.handle();

  handleEsc();
  handleArmedBeep();
}

void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
{
  button.handleEvent(aceButton, eventType, buttonState);
}

void handleEsc()
{
  if (!throttle.isArmed())
  {
    esc.writeMicroseconds(ESC_MIN_PWM);
    return;
  }

  int pulseWidth = power.getPwm();
  esc.writeMicroseconds(pulseWidth);
}

void checkCanbus()
{
    // Check if there are messages in the TWAI receive queue
    if (twai_receive(&canMsg, pdMS_TO_TICKS(0)) == ESP_OK) {
        canbus.parseCanMsg(&canMsg);
    }
    hobbywing.announce();
}

bool isMotorRunning()
{
    // Check if ESC is ready and has RPM data
    if (hobbywing.isReady()) {
        uint16_t rpm = hobbywing.getRpm();
        // Consider motor running if RPM > 100 (adjust threshold as needed)
        return rpm > 10;
    }

    // Fallback: check throttle position
    // Consider motor running if throttle > 5%
    return throttle.getThrottlePercentage() > 5
     && throttle.isArmed();
}

void handleArmedBeep()
{
    static bool wasArmed = false;
    static bool wasMotorRunning = false;

    bool isArmed = throttle.isArmed();
    bool motorRunning = isMotorRunning();

    // Start continuous beep when armed and motor stops
    if (isArmed && !motorRunning && (!wasArmed || wasMotorRunning)) {
        buzzer.beepCustom(500, 255); // 500ms beep, 255 repetitions (continuous)
    }

    // Stop beep when motor starts running or throttle is disarmed
    if ((!isArmed || motorRunning) && wasArmed && !wasMotorRunning) {
        buzzer.stop();
    }

    wasArmed = isArmed;
    wasMotorRunning = motorRunning;
}
