#ifndef DISPLAY_H
#define DISPLAY_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class Scale; // Forward declaration
class FlowRate; // Forward declaration

enum class ScaleMode {
    FLOW = 0,
    TIME = 1,
    AUTO = 2
};

class Display {
public:
    Display(uint8_t sdaPin, uint8_t sclPin, Scale* scale, FlowRate* flowRate);
    bool begin();
    void update();
    void showWeight(float weight);
    void showMessage(const String& message, int duration = 2000);
    void showSleepCountdown(int seconds); // Show sleep countdown in large format
    void showSleepMessage(); // Show initial sleep message with big/small text format
    void showGoingToSleepMessage(); // Show "Touch To / Wake Up" message like WeighMyBru Ready
    void showSleepCancelledMessage(); // Show "Sleep / Cancelled" message like WeighMyBru Ready
    void showTaringMessage(); // Show "Taring..." message like WeighMyBru Ready
    void showTaredMessage(); // Show "Tared!" message like WeighMyBru Ready
    void showModeMessage(ScaleMode mode); // Show mode name like WeighMyBru Ready
    void clearMessageState(); // Clear message state to return to weight display
    void showIPAddresses(); // Show WiFi IP addresses
    void clear();
    void setBrightness(uint8_t brightness);
    
    // Mode management
    void setMode(ScaleMode mode);
    ScaleMode getMode() const;
    void nextMode();
    
    // Timer management for TIME mode
    void startTimer();
    void stopTimer();
    void resetTimer();
    bool isTimerRunning() const;
    unsigned long getTimerSeconds() const;
    
private:
    uint8_t sdaPin;
    uint8_t sclPin;
    Scale* scalePtr;
    FlowRate* flowRatePtr;
    Adafruit_SSD1306* display;
    
    static const uint8_t SCREEN_WIDTH = 128;
    static const uint8_t SCREEN_HEIGHT = 32;
    static const uint8_t OLED_RESET = -1; // Reset pin not used
    static const uint8_t SCREEN_ADDRESS = 0x3C; // Common I2C address for SSD1306
    
    unsigned long messageStartTime;
    bool showingMessage;
    String currentMessage;
    
    // Mode system
    ScaleMode currentMode;
    
    // Timer system for TIME mode
    unsigned long timerStartTime;
    unsigned long timerPausedTime;
    bool timerRunning;
    bool timerPaused;
    
    void drawWeight(float weight);
    void showWeightWithTimer(float weight); // For TIME mode display
    void setupDisplay();
};

#endif
