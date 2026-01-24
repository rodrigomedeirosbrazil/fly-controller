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
#include "Xctod/Xctod.h"
#include "WebServer/ControllerWebServer.h"
#include "Telemetry/TelemetryProvider.h"
#if USES_CAN_BUS && IS_HOBBYWING
#include "Hobbywing/Hobbywing.h"
#endif

using namespace ace_button;
#include "Button/Button.h"

// External declarations
extern TelemetryProvider* telemetry;

ControllerWebServer webServer;
Logger logger;

void setup()
{
  Serial.begin(115200);
  webServer.begin();

  analogReadResolution(ADC_RESOLUTION);
  analogSetPinAttenuation(THROTTLE_PIN, ADC_ATTENUATION);
  analogSetPinAttenuation(MOTOR_TEMPERATURE_PIN, ADC_ATTENUATION);
#if IS_XAG
  analogSetPinAttenuation(ESC_TEMPERATURE_PIN, ADC_ATTENUATION);
  analogSetPinAttenuation(BATTERY_VOLTAGE_PIN, ADC_ATTENUATION);
#endif

  // Warm-up ADC: discard first readings
  for (int i = 0; i < 10; i++) {
    analogRead(THROTTLE_PIN);
    analogRead(MOTOR_TEMPERATURE_PIN);
#if IS_XAG
    analogRead(ESC_TEMPERATURE_PIN);
    analogRead(BATTERY_VOLTAGE_PIN);
#endif
    delay(10);
  }

  xctod.init();
  logger.init();
  buzzer.setup();

  // Initialize telemetry provider
  if (telemetry && telemetry->init) {
    telemetry->init();
  }

#if USES_CAN_BUS
  // Initialize TWAI (CAN) driver with SN65HVD230
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)CAN_TX_PIN,
    (gpio_num_t)CAN_RX_PIN,
    TWAI_MODE_NORMAL
  );
  twai_timing_config_t t_config = CAN_BITRATE;
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  twai_driver_install(&g_config, &t_config, &f_config);
  twai_start();

  // Announce presence on CAN bus
  if (telemetry && telemetry->announce) {
    telemetry->announce();
  }

  // Hobbywing-specific initialization
  #if IS_HOBBYWING
  extern Hobbywing hobbywing;
  hobbywing.requestEscId();
  hobbywing.setThrottleSource(Hobbywing::throttleSourcePWM);
  hobbywing.setLedColor(Hobbywing::ledColorGreen);
  #endif
#endif

  buzzer.beepWarning();

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
#endif
  buzzer.handle();

  // Update telemetry data
  if (telemetry && telemetry->update) {
    telemetry->update();
  }

  // Update motor temperature in telemetry (read from sensor)
  if (telemetry && telemetry->getData) {
    TelemetryData* data = telemetry->getData();
    if (data) {
      float motorTempCelsius = motorTemp.getTemperature();
      data->motorTemperatureMilliCelsius = (int32_t)(motorTempCelsius * 1000.0f);
    }
  }

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
    extern twai_message_t canMsg;
    extern Canbus canbus;

    // Check if there are messages in the TWAI receive queue
    if (twai_receive(&canMsg, pdMS_TO_TICKS(0)) == ESP_OK) {
        canbus.parseCanMsg(&canMsg);
    }

    // Announce presence on CAN bus periodically
    if (telemetry && telemetry->announce) {
        telemetry->announce();
    }
#endif
}

bool isMotorRunning()
{
    if (!telemetry || !telemetry->getData) {
        // Fallback: check throttle position
        return throttle.getThrottlePercentage() > 5 && throttle.isArmed();
    }

    TelemetryData* data = telemetry->getData();
    if (!data) {
        // Fallback: check throttle position
        return throttle.getThrottlePercentage() > 5 && throttle.isArmed();
    }

    #if IS_XAG
    // XAG mode: check throttle position only (no RPM data available)
    return throttle.getThrottlePercentage() > 5 && throttle.isArmed();
    #else
    // CAN controllers: check if ESC is ready and has RPM data
    if (data->isReady && data->rpm > 10) {
        return true;
    }

    // Fallback: check throttle position
    return throttle.getThrottlePercentage() > 5 && throttle.isArmed();
    #endif
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
