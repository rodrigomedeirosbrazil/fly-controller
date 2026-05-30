#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

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
#include "BluetoothBms/BluetoothBms.h"
#include "Xctod/Xctod.h"
#include "TelemetryLogger/TelemetryLogger.h"
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
  // Silence the buzzer immediately. On warm reset the GPIO matrix can keep
  // GPIO6 routed to a still-running LEDC channel from the previous session,
  // and on cold boot the LEDC channel-1 duty register has a non-zero hardware
  // default. Driving the pin LOW as a plain GPIO breaks any retained route
  // before any other init runs, so the piezo stays silent until buzzer.setup().
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.begin(115200);
  // After a crash/reboot (e.g. stack overflow), reason is visible on next boot. ESP_RST_PANIC=4, ESP_RST_INT_WDT=5, ESP_RST_TASK_WDT=6.
  Serial.printf("[Main] esp_reset_reason=%d\n", static_cast<int>(esp_reset_reason()));

  // Initialize Settings first (loads from Preferences)
  extern Settings settings;
  settings.init();

  hourMeter.init();

  webServer.begin();

  // Initialize ADS1115 for all builds (throttle, motor temp; XAG uses Ch2/Ch3 for ESC temp and battery; Tmotor uses Ch3 for battery)
  extern ADS1115 ads1115;
  if (!ads1115.begin(I2C_SDA_PIN, I2C_SCL_PIN)) {
    DEBUG_PRINTLN("[Main] WARNING: ADS1115 init failed — throttle and temp readings unavailable");
  }

  xctod.init();
  bluetoothBms.init();
  logger.init();
  telemetryLogger.init();
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
    DEBUG_PRINT("[Main] ERROR: Failed to install TWAI driver - Error: ");
    DEBUG_PRINTLN(install_result);
  } else {
    DEBUG_PRINTLN("[Main] TWAI driver installed successfully");
  }

  esp_err_t start_result = twai_start();
  if (start_result != ESP_OK) {
    DEBUG_PRINT("[Main] ERROR: Failed to start TWAI driver - Error: ");
    DEBUG_PRINTLN(start_result);
  } else {
    DEBUG_PRINTLN("[Main] TWAI driver started successfully");

    // Wait a bit for driver to be ready
    delay(50);

    // Check driver status (using only standard fields)
    twai_status_info_t status_info;
    twai_get_status_info(&status_info);
    DEBUG_PRINT("[Main] TWAI Status - TX Errors: ");
    DEBUG_PRINT(status_info.tx_error_counter);
    DEBUG_PRINT(", RX Errors: ");
    DEBUG_PRINTLN(status_info.rx_error_counter);
  }

  // Hobbywing-specific initialization
  #if IS_HOBBYWING
  extern HobbywingCan hobbywingCan;
  hobbywingCan.requestEscId();
  hobbywingCan.setThrottleSource(HobbywingCan::throttleSourcePWM);
  hobbywingCan.setLedColor(HobbywingCan::ledColorGreen);
  #endif
#endif

  // ESP32Servo's attach() can force the LEDC low-speed clock to XTAL so the ESC
  // can hit 50 Hz at 16-bit. That retroactively changes the divider for our
  // buzzer timer (Timer 1), so we attach the ESC first, then re-apply the
  // buzzer frequency, and only then start the system-start beep.
  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_MIN_PWM);
  buzzer.recalibrate();
  buzzer.setVolume(settings.getBuzzerVolume());
  buzzer.beepSystemStart();

  // Enable task watchdog on the main loop task. If loop() stalls for more than
  // WDT_TIMEOUT_S seconds (e.g. a blocking BLE call or infinite loop), the
  // system reboots automatically. esp_task_wdt_reset() is called each iteration.
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
}

void loop()
{
  // buzzer.handle() runs first so a beep that finished during the previous
  // iteration is silenced before any potentially slow component runs.
  buzzer.handle();

  button.check();
  bluetoothBms.update();
  xctod.write();
  telemetryLogger.handle();
#if USES_CAN_BUS
  checkCanbus();
#endif
  throttle.handle();
  hourMeter.handle(throttle.isArmed(), isMotorRunning());
  motorTemp.handle();
#if IS_XAG
  extern Temperature escTemp;
  escTemp.handle();
#endif
#if IS_XAG || IS_TMOTOR
  batterySensor.handle();
#endif

  // Update telemetry data
  telemetry.update();

  // Update battery monitor (Coulomb counting, SoC)
  extern BatteryMonitor batteryMonitor;
  batteryMonitor.update();

  handleEsc();
  handleArmedBeep();

  webServer.handleClient();

  esp_task_wdt_reset();
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
    const unsigned int MAX_CAN_FRAMES_PER_TICK = 10;
    unsigned int frameCount = 0;

    // Process received CAN frames with rate limiting to prevent starvation of other tasks
    while (frameCount < MAX_CAN_FRAMES_PER_TICK && canbus.receive(&msg)) {
        frameCount++;
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
