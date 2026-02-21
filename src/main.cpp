#include <Arduino.h>

#include <ESP32Servo.h>
#if USES_CAN_BUS
#include <driver/twai.h>
#include "Canbus/Canbus.h"
#endif
#include <AceButton.h>

#include "config.h"
#include "main.h"

#include "Throttle/Throttle.h"
#include "Temperature/Temperature.h"
#include "Buzzer/Buzzer.h"
#include "Power/Power.h"
#include "BatteryMonitor/BatteryMonitor.h"
#include "Xctod/Xctod.h"
#include "WebServer/ControllerWebServer.h"
#if USES_CAN_BUS && IS_HOBBYWING
#include "Hobbywing/HobbywingCan.h"
#endif
#if USES_CAN_BUS && IS_TMOTOR
#include "Tmotor/TmotorCan.h"
#endif

using namespace ace_button;
#include "Button/Button.h"

ControllerWebServer webServer;
Logger logger;

void setup()
{
  Serial.begin(115200);

  // Initialize Settings first (loads from Preferences)
  extern Settings settings;
  settings.init();

  webServer.begin();

#if IS_TMOTOR || IS_HOBBYWING
  // Initialize ADS1115 for Tmotor and Hobbywing
  extern ADS1115 ads1115;
  ads1115.begin(I2C_SDA_PIN, I2C_SCL_PIN);
#endif

#if IS_XAG
  // Initialize built-in ADC for XAG
  analogReadResolution(ADC_RESOLUTION);
  analogSetPinAttenuation(THROTTLE_PIN, ADC_ATTENUATION);
  analogSetPinAttenuation(MOTOR_TEMPERATURE_PIN, ADC_ATTENUATION);
  analogSetPinAttenuation(ESC_TEMPERATURE_PIN, ADC_ATTENUATION);
  analogSetPinAttenuation(BATTERY_VOLTAGE_PIN, ADC_ATTENUATION);

  // Warm-up ADC: discard first readings
  for (int i = 0; i < 10; i++) {
    analogRead(THROTTLE_PIN);
    analogRead(MOTOR_TEMPERATURE_PIN);
    analogRead(ESC_TEMPERATURE_PIN);
    analogRead(BATTERY_VOLTAGE_PIN);
    delay(10);
  }
#endif

  xctod.init();
  logger.init();
  buzzer.setup();

  // Initialize battery monitor (uses settings for capacity)
  batteryMonitor.init();

  // Initialize telemetry
  telemetry.init();

#if USES_CAN_BUS
  // Initialize TWAI (CAN) driver with SN65HVD230
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)CAN_TX_PIN,
    (gpio_num_t)CAN_RX_PIN,
    TWAI_MODE_NORMAL
  );
  twai_timing_config_t t_config = CAN_BITRATE;
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  esp_err_t install_result = twai_driver_install(&g_config, &t_config, &f_config);
  if (install_result != ESP_OK) {
    Serial.print("[Main] ERROR: Failed to install TWAI driver - Error: ");
    Serial.println(install_result);
  } else {
    Serial.println("[Main] TWAI driver installed successfully");
  }

  esp_err_t start_result = twai_start();
  if (start_result != ESP_OK) {
    Serial.print("[Main] ERROR: Failed to start TWAI driver - Error: ");
    Serial.println(start_result);
  } else {
    Serial.println("[Main] TWAI driver started successfully");

    // Wait a bit for driver to be ready
    delay(50);

    // Check driver status (using only standard fields)
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    Serial.print("[Main] TWAI Status - TX Errors: ");
    Serial.print(status_info.tx_error_counter);
    Serial.print(", RX Errors: ");
    Serial.println(status_info.rx_error_counter);
  }

  // Hobbywing-specific initialization
  #if IS_HOBBYWING
  extern HobbywingCan hobbywingCan;
  hobbywingCan.requestEscId();
  hobbywingCan.setThrottleSource(HobbywingCan::throttleSourcePWM);
  hobbywingCan.setLedColor(HobbywingCan::ledColorGreen);
  #endif
#endif

  buzzer.beepSystemStart();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_MIN_PWM);
}

void loop()
{
  button.check();
  xctod.write();
#if USES_CAN_BUS
  checkCanbus();
#endif
  throttle.handle();
  motorTemp.handle();
#if IS_XAG
  extern Temperature escTemp;
  escTemp.handle();
  batterySensor.handle();
#endif
  buzzer.handle();

  // Update telemetry data
  telemetry.update();

  // Update battery monitor (Coulomb counting, SoC)
  extern BatteryMonitor batteryMonitor;
  batteryMonitor.update();

  handleEsc();
  handleArmedBeep();

  webServer.handleClient();
}

void handleButtonEvent(AceButton* aceButton, uint8_t eventType, uint8_t buttonState)
{
  button.handleEvent(aceButton, eventType, buttonState);
}

void handleEsc()
{
  if (!throttle.isArmed())
  {
    power.resetRampLimiting();
    esc.writeMicroseconds(ESC_MIN_PWM);
    return;
  }

  int pulseWidth = power.getPwm();
  esc.writeMicroseconds(pulseWidth);
}

void checkCanbus()
{
#if USES_CAN_BUS
    extern Canbus canbus;
    twai_message_t msg;

    // Process all received CAN frames
    while (canbus.receive(&msg)) {
#if IS_HOBBYWING
        extern HobbywingCan hobbywingCan;
        hobbywingCan.parseEscMessage(&msg);
#elif IS_TMOTOR
        extern TmotorCan tmotorCan;
        tmotorCan.parseEscMessage(&msg);
#endif
    }

    // Periodic ESC commands (400 Hz throttle for Tmotor, etc.)
#if IS_HOBBYWING
    extern HobbywingCan hobbywingCan;
    hobbywingCan.handle();
#elif IS_TMOTOR
    extern TmotorCan tmotorCan;
    tmotorCan.handle();
#endif

    canbus.sendNodeStatus();
#endif
}

bool isMotorRunning()
{
    return throttle.getThrottlePercentage() > 1 && throttle.isArmed();
}

void handleArmedBeep()
{
    static bool wasArmed = false;
    static bool wasMotorRunning = false;

    bool isArmed = throttle.isArmed();
    bool motorRunning = isMotorRunning();

    // Start continuous beep when armed and motor stops
    if (isArmed && !motorRunning && (!wasArmed || wasMotorRunning)) {
        buzzer.beepArmedAlert(); // Continuous intermittent beep - critical safety alert
    }

    // Stop beep when motor starts running or throttle is disarmed
    if ((!isArmed || motorRunning) && wasArmed && !wasMotorRunning) {
        buzzer.stop();
    }

    wasArmed = isArmed;
    wasMotorRunning = motorRunning;
}
