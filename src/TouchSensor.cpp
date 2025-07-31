#include "TouchSensor.h"
#include "Scale.h"
#include "Display.h"

TouchSensor::TouchSensor(uint8_t touchPin, Scale* scale) 
    : touchPin(touchPin), scalePtr(scale), displayPtr(nullptr), touchThreshold(30000), 
      lastTouchState(false), lastTouchTime(0), touchStartTime(0), debounceDelay(200),
      longPressDetected(false) {
}

void TouchSensor::begin() {
    // Set up the pin as digital input for the touch sensor module
    pinMode(touchPin, INPUT);
    Serial.println("Digital touch sensor initialized on pin " + String(touchPin));
}

void TouchSensor::update() {
    bool currentTouchState = isTouched();
    unsigned long currentTime = millis();
    
    // Check for touch state change with debouncing
    if (currentTouchState != lastTouchState) {
        if (currentTime - lastTouchTime > debounceDelay) {
            if (currentTouchState) {
                // Touch started - record start time for long press detection
                touchStartTime = currentTime;
                longPressDetected = false;
                Serial.println("Touch started");
            } else {
                // Touch ended
                if (!longPressDetected) {
                    // Short press - trigger tare
                    handleTouch();
                } else {
                    // Touch ended after long press - complete any pending mode tare
                    if (displayPtr != nullptr) {
                        displayPtr->completePendingModeTare();
                    }
                }
                longPressDetected = false;
                Serial.println("Touch ended");
            }
            lastTouchState = currentTouchState;
            lastTouchTime = currentTime;
        }
    }
    
    // Check for long press (1 second) while touch is active
    if (currentTouchState && !longPressDetected) {
        if (currentTime - touchStartTime >= 1000) {
            longPressDetected = true;
            handleLongPress();
        }
    }
}

void TouchSensor::setTouchThreshold(uint16_t threshold) {
    touchThreshold = threshold;
    Serial.println("Touch threshold set to: " + String(touchThreshold));
}

uint16_t TouchSensor::getTouchValue() {
    // For digital touch sensor modules, return the digital state as 0 or 1
    return digitalRead(touchPin) ? 1 : 0;
}

bool TouchSensor::isTouched() {
    // For digital touch sensor modules, check if the pin is HIGH
    // Most touch sensor modules output HIGH when touched
    return digitalRead(touchPin) == HIGH;
}

void TouchSensor::setDisplay(Display* display) {
    displayPtr = display;
}

void TouchSensor::handleTouch() {
    if (scalePtr != nullptr) {
        Serial.println("Touch detected! Taring scale...");
        
        // Show tare message on display if available
        if (displayPtr != nullptr) {
            displayPtr->showTaringMessage();
        }
        
        scalePtr->tare();
        Serial.println("Scale tared successfully");
        
        // Reset timer when manual tare is pressed
        if (displayPtr != nullptr) {
            displayPtr->resetTimer();
            Serial.println("Timer reset with manual tare");
        }
        
        // Reset auto sequence if in Auto mode
        if (displayPtr != nullptr) {
            displayPtr->resetAutoSequence();
        }
        
        // Show completion message on display if available
        if (displayPtr != nullptr) {
            displayPtr->showTaredMessage();
        }
    } else {
        Serial.println("Error: Scale pointer is null");
    }
}

void TouchSensor::handleLongPress() {
    Serial.println("Long press detected! Switching mode...");
    
    if (displayPtr != nullptr) {
        displayPtr->nextMode(true); // Use delayed tare
        Serial.println("Mode switched with delayed tare");
    } else {
        Serial.println("Error: Display pointer is null");
    }
}
