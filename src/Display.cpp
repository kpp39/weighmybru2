#include "Display.h"
#include "Scale.h"
#include "FlowRate.h"
#include <WiFi.h>

Display::Display(uint8_t sdaPin, uint8_t sclPin, Scale* scale, FlowRate* flowRate)
    : sdaPin(sdaPin), sclPin(sclPin), scalePtr(scale), flowRatePtr(flowRate),
      messageStartTime(0), showingMessage(false) {
    display = new Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
}

bool Display::begin() {
    // Initialize I2C with custom pins
    Wire.begin(sdaPin, sclPin);
    
    // Initialize the display
    if (!display->begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        return false;
    }
    
    setupDisplay();
    Serial.println("SSD1306 display initialized on SDA:" + String(sdaPin) + " SCL:" + String(sclPin));
    
    // Don't show startup message here - will be shown after IP addresses
    
    return true;
}

void Display::setupDisplay() {
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    display->cp437(true); // Use full 256 char 'Code Page 437' font
}

void Display::update() {
    // Check if we're showing a temporary message
    if (showingMessage) {
        if (millis() - messageStartTime > 2000) { // Default 2 second duration
            showingMessage = false;
        } else {
            return; // Keep showing the message
        }
    }
    
    // Get current weight and display it
    if (scalePtr != nullptr) {
        float weight = scalePtr->getCurrentWeight();
        showWeight(weight);
    }
}

void Display::showWeight(float weight) {
    if (showingMessage) return; // Don't override messages
    
    display->clearDisplay();
    
    // Format weight string with consistent spacing
    String weightStr;
    if (weight < 0) {
        weightStr = String(weight, 1); // Keep negative sign
    } else {
        weightStr = " " + String(weight, 1); // Add space where negative sign would be
    }
    weightStr += "g";
    
    // Calculate text width for centering weight
    display->setTextSize(2);
    int16_t x1, y1;
    uint16_t textWidth, textHeight;
    display->getTextBounds(weightStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
    
    // Center the weight text horizontally
    int centerX = (SCREEN_WIDTH - textWidth) / 2;
    
    // Large weight display - centered at top
    display->setCursor(centerX, 0);
    display->print(weightStr);
    
    // Get flow rate and format it
    float currentFlowRate = 0.0;
    if (flowRatePtr != nullptr) {
        currentFlowRate = flowRatePtr->getFlowRate();
    }
    
    // Format flow rate string with consistent spacing
    String flowRateStr = "Flow Rate: ";
    if (currentFlowRate < 0) {
        flowRateStr += String(currentFlowRate, 1); // Keep negative sign
    } else {
        flowRateStr += " " + String(currentFlowRate, 1); // Add space where negative sign would be
    }
    flowRateStr += "g/s";
    
    // Small flow rate text at bottom - centered
    display->setTextSize(1);
    display->getTextBounds(flowRateStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int flowRateCenterX = (SCREEN_WIDTH - textWidth) / 2;
    display->setCursor(flowRateCenterX, 24);
    display->print(flowRateStr);
    
    display->display();
}

void Display::showMessage(const String& message, int duration) {
    currentMessage = message;
    messageStartTime = millis();
    showingMessage = true;
    
    display->clearDisplay();
    display->setTextSize(1);
    display->setCursor(0, 0);
    
    // Word wrap for longer messages
    int lineHeight = 8;
    int maxCharsPerLine = 21; // For 128px width
    int currentLine = 0;
    
    for (int i = 0; i < message.length() && currentLine < 4; i += maxCharsPerLine) {
        String line = message.substring(i, min(i + maxCharsPerLine, (int)message.length()));
        display->setCursor(0, currentLine * lineHeight);
        display->print(line);
        currentLine++;
    }
    
    display->display();
    
    // Update duration for this message
    if (duration > 0) {
        // We'll check this in update() method
    }
}

void Display::showIPAddresses() {
    display->clearDisplay();
    display->setTextSize(1);
    
    String apIP = "AP: " + WiFi.softAPIP().toString();
    String staIP = "";
    
    // Check if connected to WiFi station mode
    if (WiFi.status() == WL_CONNECTED) {
        staIP = "STA: " + WiFi.localIP().toString();
    } else {
        staIP = "STA: Not Connected";
    }
    
    // Display AP IP
    display->setCursor(0, 0);
    display->print(apIP);
    
    // Display STA IP
    display->setCursor(0, 12);
    display->print(staIP);
    
    // Show for 3 seconds
    display->display();
    delay(3000);
    
    // Now show the WeighMyBru Ready message for 3 seconds
    showMessage("WeighMyBru Ready!", 3000);
}

void Display::clear() {
    display->clearDisplay();
    display->display();
}

void Display::setBrightness(uint8_t brightness) {
    // SSD1306 doesn't have brightness control, but we can simulate with contrast
    display->ssd1306_command(SSD1306_SETCONTRAST);
    display->ssd1306_command(brightness);
}
