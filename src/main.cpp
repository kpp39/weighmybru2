#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "Arduino.h"
#if (ARDUINO_USB_CDC_ON_BOOT|ARDUINO_USB_MSC_ON_BOOT|ARDUINO_USB_DFU_ON_BOOT) && !ARDUINO_USB_MODE
#include "USB.h"
#if ARDUINO_USB_MSC_ON_BOOT
#include "FirmwareMSC.h"
#endif
#endif


#ifndef ARDUINO_LOOP_STACK_SIZE
#ifndef CONFIG_ARDUINO_LOOP_STACK_SIZE
#define ARDUINO_LOOP_STACK_SIZE 8192
#else
#define ARDUINO_LOOP_STACK_SIZE CONFIG_ARDUINO_LOOP_STACK_SIZE
#endif
#endif

TaskHandle_t loopTaskHandle = NULL;

#if CONFIG_AUTOSTART_ARDUINO
#if CONFIG_FREERTOS_UNICORE
void yieldIfNecessary(void){
    static uint64_t lastYield = 0;
    uint64_t now = millis();
    if((now - lastYield) > 2000) {
        lastYield = now;
        vTaskDelay(5); //delay 1 RTOS tick
    }
}
#endif

bool loopTaskWDTEnabled;

__attribute__((weak)) size_t getArduinoLoopTaskStackSize(void) {
    return ARDUINO_LOOP_STACK_SIZE;
}

void loopTask(void *pvParameters)
{
    setup();
    for(;;) {
#if CONFIG_FREERTOS_UNICORE
        yieldIfNecessary();
#endif
        if(loopTaskWDTEnabled){
            esp_task_wdt_reset();
        }
        loop();
        if (serialEventRun) serialEventRun();
    }
}

extern "C" void app_main()
{
#if ARDUINO_USB_CDC_ON_BOOT && !ARDUINO_USB_MODE
    Serial.begin();
#endif
#if ARDUINO_USB_MSC_ON_BOOT && !ARDUINO_USB_MODE
    MSC_Update.begin();
#endif
#if ARDUINO_USB_DFU_ON_BOOT && !ARDUINO_USB_MODE
    USB.enableDFU();
#endif
#if ARDUINO_USB_ON_BOOT && !ARDUINO_USB_MODE
    USB.begin();
#endif
    loopTaskWDTEnabled = false;
    initArduino();
    xTaskCreateUniversal(loopTask, "loopTask", getArduinoLoopTaskStackSize(), NULL, 1, &loopTaskHandle, ARDUINO_RUNNING_CORE);
}

#endif


#include <WiFi.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <esp_sleep.h>
#include "WebServer.h"
#include "Scale.h"
#include "WiFiManager.h"
#include <ESPAsyncWebServer.h>
#include "FlowRate.h"
#include "Calibration.h"
#include "BluetoothScale.h"
#include "TouchSensor.h"
#include "Display.h"
#include "PowerManager.h"
#include "BatteryMonitor.h"
#include <esp_pm.h>

// Pins and calibration
uint8_t dataPin = 5;   // HX711 Data pin (moved from 12)
uint8_t clockPin = 6;  // HX711 Clock pin (moved from 11)
uint8_t touchPin = 4;  // T0 - Confirmed working touch pin for tare
uint8_t sleepTouchPin = 3;  // GPIO3 - Digital touch sensor for sleep functionality
uint8_t batteryPin = 7;    // GPIO7 - Battery voltage monitoring (ADC1_CH6) - Safe GPIO
uint8_t sdaPin = 8;    // I2C Data pin for display (moved from 5)
uint8_t sclPin = 9;    // I2C Clock pin for display (moved from 6)
float calibrationFactor = 4195.712891;
Scale scale(dataPin, clockPin, calibrationFactor);
FlowRate flowRate;
BluetoothScale bluetoothScale;
TouchSensor touchSensor(touchPin, &scale);
Display oledDisplay(sdaPin, sclPin, &scale, &flowRate);
PowerManager powerManager(sleepTouchPin, &oledDisplay);
BatteryMonitor batteryMonitor(batteryPin);
void uiUpdate(void * parameter){
  TickType_t lastTick = xTaskGetTickCount();
  for(;;){
    touchSensor.update();
    powerManager.update();
    batteryMonitor.update();
    oledDisplay.update();
    xTaskDelayUntil(&lastTick, 25 / portTICK_PERIOD_MS);
  }
}



void weightUpdate(void * parameter){
  TickType_t lastTick = xTaskGetTickCount();
  for(;;){
    float weight = scale.getWeight();
    flowRate.update(weight);
    xTaskDelayUntil(&lastTick, 50 / portTICK_PERIOD_MS);
  }
}
void bleUpdate(void * parameter){
  TickType_t lastTick = xTaskGetTickCount();
  for(;;){
    bluetoothScale.update();
    xTaskDelayUntil(&lastTick, 50 / portTICK_PERIOD_MS);
  }
}
void checkWiFiStatus(void * parameter){
  TickType_t lastTick = xTaskGetTickCount();
  for(;;){
    float weight = scale.getWeight();
    flowRate.update(weight);
    xTaskDelayUntil(&lastTick, 50 / portTICK_PERIOD_MS);
  }
}
void setupWebServer(Scale &scale, FlowRate &flowRate, BluetoothScale &bluetoothScale, Display &display, BatteryMonitor &battery);
void setup() {
  Serial.begin(115200);
  
  // Link scale and flow rate for tare operation coordination
  scale.setFlowRatePtr(&flowRate);
  
  // Check for factory reset request (hold touch pin during boot)
  pinMode(touchPin, INPUT_PULLDOWN);
  if (digitalRead(touchPin) == HIGH) {
    Serial.println("FACTORY RESET: Touch pin held during boot - clearing WiFi credentials");
    clearWiFiCredentials();
    delay(1000);
  }
  
  // CRITICAL: Initialize BLE FIRST before WiFi to prevent radio conflicts
  Serial.println("Initializing BLE FIRST for GaggiMate compatibility...");
  Serial.printf("Free heap before BLE init: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM before BLE init: %u bytes\n", ESP.getFreePsram());
  
  try {
    bluetoothScale.begin();  // Initialize BLE without scale reference
    Serial.println("BLE initialized successfully - GaggiMate should be able to connect");
    Serial.printf("Free heap after BLE init: %u bytes\n", ESP.getFreeHeap());
    Serial.printf("Free PSRAM after BLE init: %u bytes\n", ESP.getFreePsram());
  } catch (...) {
    Serial.println("BLE initialization failed - continuing without Bluetooth");
    Serial.printf("Free heap after BLE fail: %u bytes\n", ESP.getFreeHeap());
  }
  
  // Initialize display with error handling - don't block if display fails
  Serial.println("Initializing display...");
  bool displayAvailable = oledDisplay.begin();
  
  if (!displayAvailable) {
    Serial.println("WARNING: Display initialization failed!");
    Serial.println("System will continue in headless mode without display.");
    Serial.println("All functionality remains available via web interface.");
  } else {
    Serial.println("Display initialized - ready for visual feedback");
  }
  
  // Check wake-up reason and show appropriate message
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal (touch sensor)");
      // Show the same starting message as normal boot for consistency
      delay(1500);
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    default:
      Serial.println("Wakeup was not caused by deep sleep: " + String(wakeup_reason));
      // For normal startup, the begin() method already shows a startup message
      delay(1000);
      break;
  }
  //Wait for BLE to finish intitalizing before starting WiFi
  delay(1500); 
  
  // ALWAYS enable WiFi power management for optimal battery life
  // This works regardless of WiFi mode (STA/AP/OFF) and should be set early
  WiFi.setSleep(true);
  Serial.println("WiFi power management enabled for battery optimization");
  
  setupWiFi();

  // Wait for WiFi to fully stabilize after BLE is already running
  delay(1500);
  Serial.printf("Version: %s\n", ESP.getSdkVersion());
  // Initialize scale with error handling - don't block web server if HX711 fails
  Serial.println("Initializing scale...");
  if (!scale.begin()) {
    Serial.println("WARNING: Scale (HX711) initialization failed!");
    Serial.println("Web server will continue to run, but scale readings will not be available.");
    Serial.println("Check HX711 wiring and connections.");
  } else {
    Serial.println("Scale initialized successfully");
    // Now that scale is ready, set the reference in BluetoothScale
    bluetoothScale.setScale(&scale);
  }
  
  // BLE was initialized earlier - no need to initialize again
  // bluetoothScale.begin(&scale);
  
  // Set bluetooth reference in display for status indicator (if display available)
  if (oledDisplay.isConnected()) {
    oledDisplay.setBluetoothScale(&bluetoothScale);
  }
  
  // Set display reference in bluetooth for timer control
  bluetoothScale.setDisplay(&oledDisplay);
  
  // Set power manager reference in display for timer state synchronization (if display available)
  if (oledDisplay.isConnected()) {
    oledDisplay.setPowerManager(&powerManager);
  }
  
  // Set battery monitor reference in display for battery status (if display available)
  if (oledDisplay.isConnected()) {
    oledDisplay.setBatteryMonitor(&batteryMonitor);
  }

  // Initialize touch sensor
  touchSensor.begin();

  // Initialize power manager
  powerManager.begin();

  // Initialize battery monitor
  batteryMonitor.begin();

  // Show IP addresses and welcome message if display is available
  delay(100); // Small delay to ensure WiFi is fully initialized
  if (oledDisplay.isConnected()) {
    oledDisplay.showIPAddresses();
  }

  // Link display to touch sensor for tare feedback (if display available)
  if (oledDisplay.isConnected()) {
    touchSensor.setDisplay(&oledDisplay);
  }
  
  // Link flow rate to touch sensor for averaging reset on tare
  touchSensor.setFlowRate(&flowRate);

  setupWebServer(scale, flowRate, bluetoothScale, oledDisplay, batteryMonitor);
  xTaskCreate (
    weightUpdate,
    "Weight update",
    10000,
    NULL,
    1,
    NULL
  );
  xTaskCreate (
    bleUpdate,
    "Update Bluetooth",
    10000,
    NULL,
    1,
    NULL
  );
  
  xTaskCreate (
    printWiFiStatus,
    "Check WiFi status",
    10000,
    NULL,
    1,
    NULL
  );
  xTaskCreate (
    maintainWiFi,
    "Maintain WiFi AP stability",
    10000,
    NULL,
    1,
    NULL
  );
   xTaskCreate (
    uiUpdate,
    "UI update",
    10000,
    NULL,
    1,
    NULL
  );
  esp_pm_config_esp32s3_t pm_config = {
        .max_freq_mhz = 240, // Maximum CPU frequency when needed
        .min_freq_mhz = 80,  // Minimum CPU frequency when idle
        .light_sleep_enable = false // Enable automatic light sleep
    };
  // esp_pm_configure(&pm_config);
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

}

void loop() {vTaskDelay(250);}
// void loop() {vTaskDelete(NULL);}

