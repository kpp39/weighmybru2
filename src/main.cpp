#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "WebServer.h"
#include "Scale.h"
#include "WiFiManager.h"
#include "FlowRate.h"
#include "Calibration.h"
#include "BluetoothScale.h"
#include "TouchSensor.h"
#include "Display.h"

// Pins and calibration
uint8_t dataPin = 12;
uint8_t clockPin = 11;
uint8_t touchPin = 4;  // T0 - Confirmed working touch pin
uint8_t sdaPin = 5;    // I2C Data pin for display (same side as other pins)
uint8_t sclPin = 6;    // I2C Clock pin for display (same side as other pins)
float calibrationFactor = 4762.1621;
Scale scale(dataPin, clockPin, calibrationFactor);
FlowRate flowRate;
BluetoothScale bluetoothScale;
TouchSensor touchSensor(touchPin, &scale);
Display oledDisplay(sdaPin, sclPin, &scale, &flowRate);

void setup() {
  Serial.begin(115200);

  setupWiFi();

  scale.begin();
  
  // Initialize Bluetooth scale
  bluetoothScale.begin(&scale);

  // Initialize touch sensor
  touchSensor.begin();

  // Initialize display
  if (!oledDisplay.begin()) {
    Serial.println("Display initialization failed!");
  } else {
    // Show IP addresses after WiFi setup
    delay(100); // Small delay to ensure WiFi is fully initialized
    oledDisplay.showIPAddresses();
  }

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
  
  // Update display
  oledDisplay.update();
  
  // Shorter delay for more responsive weight readings
  delay(5); // Reduced from 100ms to 5ms for brewing responsiveness
}
