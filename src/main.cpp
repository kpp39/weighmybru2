#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_sleep.h>
#include "WebServer.h"
#include "Scale.h"
#include "WiFiManager.h"
#include "FlowRate.h"
#include "Calibration.h"
#include "BluetoothScale.h"
#include "TouchSensor.h"
#include "Display.h"
#include "PowerManager.h"

// Pins and calibration
uint8_t dataPin = 5;   // HX711 Data pin (moved from 12)
uint8_t clockPin = 6;  // HX711 Clock pin (moved from 11)
uint8_t touchPin = 4;  // T0 - Confirmed working touch pin for tare
uint8_t sleepTouchPin = 3;  // GPIO3 - Digital touch sensor for sleep functionality
uint8_t sdaPin = 8;    // I2C Data pin for display (moved from 5)
uint8_t sclPin = 9;    // I2C Clock pin for display (moved from 6)
float calibrationFactor = 4195.712891;
Scale scale(dataPin, clockPin, calibrationFactor);
FlowRate flowRate;
BluetoothScale bluetoothScale;
TouchSensor touchSensor(touchPin, &scale);
Display oledDisplay(sdaPin, sclPin, &scale, &flowRate);
PowerManager powerManager(sleepTouchPin, &oledDisplay);

void setup() {
  Serial.begin(115200);
  
  // Initialize display FIRST for immediate visual feedback
  if (!oledDisplay.begin()) {
    Serial.println("Display initialization failed!");
  } else {
    Serial.println("Display initialized - ready for messages");
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

  setupWiFi();

  scale.begin();
  
  // Initialize Bluetooth scale
  bluetoothScale.begin(&scale);
  
  // Set bluetooth reference in display for status indicator
  oledDisplay.setBluetoothScale(&bluetoothScale);
  
  // Set power manager reference in display for timer state synchronization
  oledDisplay.setPowerManager(&powerManager);

  // Initialize touch sensor
  touchSensor.begin();

  // Initialize power manager
  powerManager.begin();

  // Show IP addresses and welcome message now that everything is ready
  delay(100); // Small delay to ensure WiFi is fully initialized
  oledDisplay.showIPAddresses();

  // Link display to touch sensor for tare feedback
  touchSensor.setDisplay(&oledDisplay);

  setupWebServer(scale, flowRate, bluetoothScale);
}

void loop() {
  static unsigned long lastWeightUpdate = 0;
  
  // Update weight more frequently for brewing accuracy
  if (millis() - lastWeightUpdate >= 10) { // Update every 10ms
    float weight = scale.getWeight();
    flowRate.update(weight);
    lastWeightUpdate = millis();
  }
  
  // Update Bluetooth scale
  bluetoothScale.update();
  
  // Update touch sensor
  touchSensor.update();
  
  // Update power manager
  powerManager.update();
  
  // Update display
  oledDisplay.update();
  
  // Shorter delay for more responsive weight readings
  delay(5); // Reduced from 100ms to 5ms for brewing responsiveness
}
