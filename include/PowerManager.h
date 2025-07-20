#ifndef POWERMANAGER_H
#define POWERMANAGER_H

#include <Arduino.h>
#include <esp_sleep.h>

class Display; // Forward declaration

class PowerManager {
public:
    PowerManager(uint8_t sleepTouchPin, Display* display = nullptr);
    void begin();
    void update();
    void enterDeepSleep();
    void setSleepTouchThreshold(uint16_t threshold);
    bool isSleepTouchPressed();
    void setDisplay(Display* display);
    
private:
    uint8_t sleepTouchPin;
    Display* displayPtr;
    uint16_t sleepTouchThreshold;
    bool lastSleepTouchState;
    unsigned long lastSleepTouchTime;
    unsigned long touchStartTime;
    unsigned long debounceDelay;
    unsigned long sleepCountdownStart;
    bool sleepCountdownActive;
    bool longPressDetected;
    bool cancelledRecently;
    unsigned long cancelTime;
    
    void handleSleepTouch();
    void showSleepCountdown(int seconds);
};

#endif
