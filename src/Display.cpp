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
    // First show the WeighMyBru Ready message for 3 seconds
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Calculate text positioning for centering
    String line1 = "WeighMyBru";
    String line2 = "Ready";
    
    int16_t x1, y1;
    uint16_t w1, h1, w2, h2;
    
    // Get text bounds for both lines
    display->getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    display->getTextBounds(line2, 0, 0, &x1, &y1, &w2, &h2);
    
    // Calculate centered positions - tighter spacing for size 2 text
    int centerX1 = (SCREEN_WIDTH - w1) / 2;
    int centerX2 = (SCREEN_WIDTH - w2) / 2;
    
    // Position lines closer together to fit in 32 pixels
    int line1Y = 0;  // Start at top
    int line2Y = 16; // Second line at pixel 16
    
    // Display first line
    display->setCursor(centerX1, line1Y);
    display->print(line1);
    
    // Display second line
    display->setCursor(centerX2, line2Y);
    display->print(line2);
    
    display->display();
    delay(3000);
    
    // Then show IP addresses
    display->clearDisplay();
    display->setTextSize(1);
    display->setTextColor(SSD1306_WHITE);
    
    String apIP = "AP: " + WiFi.softAPIP().toString();
    String staIP = "";
    
    // Check if connected to WiFi station mode
    if (WiFi.status() == WL_CONNECTED) {
        staIP = "STA: " + WiFi.localIP().toString();
    } else {
        staIP = "STA: Not Connected";
    }
    
    // Calculate text positioning for centering IP addresses
    int16_t ipX1, ipY1;
    uint16_t ipW1, ipH1, ipW2, ipH2;
    
    // Get text bounds for both IP lines
    display->getTextBounds(apIP, 0, 0, &ipX1, &ipY1, &ipW1, &ipH1);
    display->getTextBounds(staIP, 0, 0, &ipX1, &ipY1, &ipW2, &ipH2);
    
    // Calculate centered positions
    int ipCenterX1 = (SCREEN_WIDTH - ipW1) / 2;
    int ipCenterX2 = (SCREEN_WIDTH - ipW2) / 2;
    
    // Position lines to fit nicely in 32 pixels
    int ipLine1Y = 4;   // Start a bit from top
    int ipLine2Y = 20;  // Second line lower
    
    // Display AP IP - centered
    display->setCursor(ipCenterX1, ipLine1Y);
    display->print(apIP);
    
    // Display STA IP - centered
    display->setCursor(ipCenterX2, ipLine2Y);
    display->print(staIP);
    
    // Show for 3 seconds
    display->display();
    delay(3000);
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
