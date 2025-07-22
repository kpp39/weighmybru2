#include "Display.h"
#include "Scale.h"
#include "FlowRate.h"
#include "BluetoothScale.h"
#include <WiFi.h>

Display::Display(uint8_t sdaPin, uint8_t sclPin, Scale* scale, FlowRate* flowRate)
    : sdaPin(sdaPin), sclPin(sclPin), scalePtr(scale), flowRatePtr(flowRate), bluetoothPtr(nullptr),
      messageStartTime(0), messageDuration(2000), showingMessage(false), currentMode(ScaleMode::FLOW),
      timerStartTime(0), timerPausedTime(0), timerRunning(false), timerPaused(false),
      lastWeight(0.0), lastWeightChangeTime(0), waitingForStabilization(false),
      weightWhenChanged(0.0), stabilizationStartTime(0), autoTareEnabled(true),
      lastFlowRate(0.0), autoTimerStarted(false) {
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
    
    // Show startup message in same format as welcome message
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    String line1 = "WeighMyBru";
    String line2 = "Starting";
    
    int16_t x1, y1;
    uint16_t w1, h1, w2, h2;
    
    // Get text bounds for both lines
    display->getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    display->getTextBounds(line2, 0, 0, &x1, &y1, &w2, &h2);
    
    // Calculate centered positions
    int centerX1 = (SCREEN_WIDTH - w1) / 2;
    int centerX2 = (SCREEN_WIDTH - w2) / 2;
    
    // Position lines to fit in 32 pixels
    int line1Y = 0;  // Start at top
    int line2Y = 16; // Second line at pixel 16
    
    // Display first line
    display->setCursor(centerX1, line1Y);
    display->print(line1);
    
    // Display second line
    display->setCursor(centerX2, line2Y);
    display->print(line2);
    
    display->display();
    
    Serial.println("SSD1306 display initialized on SDA:" + String(sdaPin) + " SCL:" + String(sclPin));
    
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
        if (millis() - messageStartTime > messageDuration) { // Use stored duration
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
    
    // Handle different modes
    switch (currentMode) {
        case ScaleMode::FLOW:
            drawWeight(weight); // Use existing flow rate display
            break;
        case ScaleMode::TIME:
            showWeightWithTimer(weight); // New timer display
            break;
        case ScaleMode::AUTO:
            showWeightWithFlowAndTimer(weight); // Show both flow rate and timer
            break;
    }
}

void Display::showMessage(const String& message, int duration) {
    currentMessage = message;
    messageStartTime = millis();
    messageDuration = duration; // Store the duration
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

void Display::showSleepCountdown(int seconds) {
    // Set message state to prevent weight display interference
    currentMessage = "Sleep countdown active";
    messageStartTime = millis();
    showingMessage = true;
    
    // Show countdown in same large format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    String line1 = "Sleep in";
    String line2 = String(seconds) + "...";
    
    int16_t x1, y1;
    uint16_t w1, h1, w2, h2;
    
    // Get text bounds for both lines
    display->getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    display->getTextBounds(line2, 0, 0, &x1, &y1, &w2, &h2);
    
    // Calculate centered positions
    int centerX1 = (SCREEN_WIDTH - w1) / 2;
    int centerX2 = (SCREEN_WIDTH - w2) / 2;
    
    // Position lines to fit in 32 pixels
    int line1Y = 0;  // Start at top
    int line2Y = 16; // Second line at pixel 16
    
    // Display first line
    display->setCursor(centerX1, line1Y);
    display->print(line1);
    
    // Display second line
    display->setCursor(centerX2, line2Y);
    display->print(line2);
    
    display->display();
}

void Display::showSleepMessage() {
    // Set message state to prevent weight display interference
    currentMessage = "Sleep message active";
    messageStartTime = millis();
    showingMessage = true;
    
    // Show sleep message with large top line and small bottom line
    display->clearDisplay();
    display->setTextColor(SSD1306_WHITE);
    
    // First line: "Sleep in 3" in large text (size 2)
    display->setTextSize(2);
    String line1 = "Sleeping..";
    
    int16_t x1, y1;
    uint16_t w1, h1;
    display->getTextBounds(line1, 0, 0, &x1, &y1, &w1, &h1);
    int centerX1 = (SCREEN_WIDTH - w1) / 2;
    
    display->setCursor(centerX1, 0);
    display->print(line1);
    
    // Second line: "Touch to cancel" in small text (size 1)
    display->setTextSize(1);
    String line2 = "Touch to cancel";
    
    uint16_t w2, h2;
    display->getTextBounds(line2, 0, 0, &x1, &y1, &w2, &h2);
    int centerX2 = (SCREEN_WIDTH - w2) / 2;
    
    // Position small text at bottom (24 pixels from top gives us 8 pixels for the text)
    display->setCursor(centerX2, 24);
    display->print(line2);
    
    display->display();
}

void Display::showGoingToSleepMessage() {
    // Set message state to prevent weight display interference
    currentMessage = "Going to sleep message";
    messageStartTime = millis();
    showingMessage = true;
    
    // Show "Touch To / Wake Up" in same format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Calculate text positioning for centering
    String line1 = "Touch To";
    String line2 = "Wake Up";
    
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
}

void Display::showSleepCancelledMessage() {
    // Set message state to prevent weight display interference
    currentMessage = "Sleep cancelled message";
    messageStartTime = millis();
    showingMessage = true;
    
    // Show "Sleep / Cancelled" in same format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Calculate text positioning for centering
    String line1 = "Sleep";
    String line2 = "Cancelled";
    
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
}

void Display::showTaringMessage() {
    // Set message state to prevent weight display interference
    currentMessage = "Taring message";
    messageStartTime = millis();
    showingMessage = true;
    
    // Show "Taring..." in same format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Since "Taring..." is a single word, we'll center it on one line
    // For consistency with WeighMyBru style, we can split it as "Taring" and "..."
    String line1 = "Taring";
    String line2 = "...";
    
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
}

void Display::showTaredMessage() {
    // Set message state to prevent weight display interference
    currentMessage = "Tared message";
    messageStartTime = millis();
    showingMessage = true;
    
    // Show "Tared!" in same format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Split "Tared!" into two lines for better visual impact
    String line1 = "Scale";
    String line2 = "Tared!";
    
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
}

void Display::showAutoTaredMessage() {
    // Set message state to prevent weight display interference
    currentMessage = "Auto Tared message";
    messageStartTime = millis();
    messageDuration = 1500; // Show for 1.5 seconds (reduced from 3 seconds)
    showingMessage = true;
    
    // Show "Auto Tared!" in same format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Split "Auto Tared!" into two lines for better visual impact
    String line1 = "Auto";
    String line2 = "Tared!";
    
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
}

void Display::clearMessageState() {
    showingMessage = false;
    currentMessage = "";
    messageStartTime = 0;
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
    delay(1000);
    
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
    delay(2000);
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

void Display::setBluetoothScale(BluetoothScale* bluetooth) {
    bluetoothPtr = bluetooth;
}

void Display::drawBluetoothStatus() {
    // Only draw if we have a bluetooth instance and it's connected
    if (bluetoothPtr && bluetoothPtr->isConnected()) {
        // Draw a small "BT" in the top-right corner
        display->setTextSize(1);
        display->setCursor(108, 0); // Position at top-right (128-20=108 pixels from left)
        display->print("BT");
    }
}

void Display::drawWeight(float weight) {
    display->clearDisplay();
    
    // Apply deadband to prevent flickering between 0.0g and -0.0g
    // Show 0.0g (without negative sign) when weight is between -0.1g and +0.1g
    float displayWeight = weight;
    if (weight >= -0.1 && weight <= 0.1) {
        displayWeight = 0.0; // Force to exactly 0.0 to avoid negative sign
    }
    
    // Format weight string with consistent spacing
    String weightStr;
    if (displayWeight < 0) {
        weightStr = String(displayWeight, 1); // Keep negative sign for values below -0.1g
    } else {
        weightStr = " " + String(displayWeight, 1); // Add space where negative sign would be
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
    
    // Apply deadband to prevent flickering between 0.0g/s and -0.0g/s
    // Show 0.0g/s (without negative sign) when flow rate is between -0.1g/s and +0.1g/s
    float displayFlowRate = currentFlowRate;
    if (currentFlowRate >= -0.1 && currentFlowRate <= 0.1) {
        displayFlowRate = 0.0; // Force to exactly 0.0 to avoid negative sign
    }
    
    // Format flow rate string with consistent spacing (shorter format like Auto mode)
    String flowRateStr = "FR: ";
    if (displayFlowRate < 0) {
        flowRateStr += String(displayFlowRate, 1); // Keep negative sign for values below -0.1g/s
    } else {
        flowRateStr += String(displayFlowRate, 1); // No extra space needed for shorter format
    }
    flowRateStr += "g/s";
    
    // Small flow rate text at bottom left (same as Auto mode)
    display->setTextSize(1);
    display->setCursor(0, 24);
    display->print(flowRateStr);
    
    // Draw Bluetooth status if connected
    drawBluetoothStatus();
    
    display->display();
}

void Display::showWeightWithTimer(float weight) {
    display->clearDisplay();
    
    // Apply deadband to prevent flickering between 0.0g and -0.0g
    float displayWeight = weight;
    if (weight >= -0.1 && weight <= 0.1) {
        displayWeight = 0.0;
    }
    
    // Format weight string with consistent spacing
    String weightStr;
    if (displayWeight < 0) {
        weightStr = String(displayWeight, 1);
    } else {
        weightStr = " " + String(displayWeight, 1);
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
    
    // Show timer instead of flow rate with decimal precision
    float currentTime = getTimerSeconds();
    String timerStr = "Timer: " + String(currentTime, 1) + "s";
    
    // Small timer text at bottom - centered
    display->setTextSize(1);
    display->getTextBounds(timerStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int timerCenterX = (SCREEN_WIDTH - textWidth) / 2;
    display->setCursor(timerCenterX, 24);
    display->print(timerStr);
    
    // Draw Bluetooth status if connected
    drawBluetoothStatus();
    
    display->display();
}

void Display::showWeightWithFlowAndTimer(float weight) {
    // Don't run auto-tare checks if we're showing a message
    if (!showingMessage) {
        checkAutoTare(weight);
    }
    
    // If we're showing a message, don't override it with weight display
    if (showingMessage) {
        return;
    }
    
    display->clearDisplay();
    
    // Apply deadband to prevent flickering between 0.0g and -0.0g
    float displayWeight = weight;
    if (weight >= -0.1 && weight <= 0.1) {
        displayWeight = 0.0;
    }
    
    // Format weight string with consistent spacing
    String weightStr;
    if (displayWeight < 0) {
        weightStr = String(displayWeight, 1);
    } else {
        weightStr = " " + String(displayWeight, 1);
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
    
    // Get flow rate and format it for left side
    float currentFlowRate = 0.0;
    if (flowRatePtr != nullptr) {
        currentFlowRate = flowRatePtr->getFlowRate();
    }
    
    // Auto timer functionality - check flow rate changes
    checkAutoTimer(currentFlowRate);
    
    // Apply deadband to flow rate
    float displayFlowRate = currentFlowRate;
    if (currentFlowRate >= -0.1 && currentFlowRate <= 0.1) {
        displayFlowRate = 0.0;
    }
    
    // Format flow rate string (shorter format for space)
    String flowRateStr = "FR: ";
    if (displayFlowRate < 0) {
        flowRateStr += String(displayFlowRate, 1);
    } else {
        flowRateStr += String(displayFlowRate, 1);
    }
    flowRateStr += "g/s";
    
    // Get timer for right side
    float currentTime = getTimerSeconds();
    String timerStr = String(currentTime, 1) + "s";
    
    // Small text at bottom - flow rate on left, timer on right
    display->setTextSize(1);
    
    // Flow rate on bottom left
    display->setCursor(0, 24);
    display->print(flowRateStr);
    
    // Timer on bottom right (adjusted for Bluetooth indicator if present)
    display->getTextBounds(timerStr, 0, 0, &x1, &y1, &textWidth, &textHeight);
    int timerX = SCREEN_WIDTH - textWidth;
    // If Bluetooth is connected, move timer left a bit to avoid overlap
    if (bluetoothPtr && bluetoothPtr->isConnected()) {
        timerX -= 25; // Move 25 pixels left to avoid "BT" text
    }
    display->setCursor(timerX, 24);
    display->print(timerStr);
    
    // Draw Bluetooth status if connected
    drawBluetoothStatus();
    
    display->display();
}

void Display::showModeMessage(ScaleMode mode) {
    // Set message state to prevent weight display interference
    currentMessage = "Mode change message";
    messageStartTime = millis();
    messageDuration = 1000; // Show for 1 second (reduced from default 2 seconds)
    showingMessage = true;
    
    // Show mode name in same format as WeighMyBru Ready
    display->clearDisplay();
    display->setTextSize(2);
    display->setTextColor(SSD1306_WHITE);
    
    // Get mode name
    String line1, line2;
    switch (mode) {
        case ScaleMode::FLOW:
            line1 = "Flow";
            line2 = "Mode";
            break;
        case ScaleMode::TIME:
            line1 = "Time";
            line2 = "Mode";
            break;
        case ScaleMode::AUTO:
            line1 = "Auto";
            line2 = "Mode";
            break;
    }
    
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
}

// Mode management methods
void Display::setMode(ScaleMode mode) {
    currentMode = mode;
    
    // Reset auto mode variables when switching to AUTO mode
    if (mode == ScaleMode::AUTO) {
        autoTareEnabled = true;
        waitingForStabilization = false;
        autoTimerStarted = false;
        lastWeight = 0.0;
        lastFlowRate = 0.0;
        Serial.println("Auto mode activated - auto-tare and auto-timer enabled");
    }
    
    showModeMessage(mode); // Show mode change message
}

ScaleMode Display::getMode() const {
    return currentMode;
}

void Display::nextMode() {
    switch (currentMode) {
        case ScaleMode::FLOW:
            setMode(ScaleMode::TIME);
            break;
        case ScaleMode::TIME:
            setMode(ScaleMode::AUTO);
            break;
        case ScaleMode::AUTO:
            setMode(ScaleMode::FLOW);
            break;
    }
}

// Timer management methods
void Display::startTimer() {
    if (!timerRunning) {
        timerStartTime = millis();
        timerRunning = true;
        timerPaused = false;
    }
}

void Display::stopTimer() {
    if (timerRunning && !timerPaused) {
        timerPausedTime = millis() - timerStartTime;
        timerPaused = true;
    }
}

void Display::resetTimer() {
    timerStartTime = 0;
    timerPausedTime = 0;
    timerRunning = false;
    timerPaused = false;
}

bool Display::isTimerRunning() const {
    return timerRunning && !timerPaused;
}

float Display::getTimerSeconds() const {
    if (!timerRunning) {
        return 0.0;
    } else if (timerPaused) {
        return timerPausedTime / 1000.0;
    } else {
        return (millis() - timerStartTime) / 1000.0;
    }
}

void Display::checkAutoTare(float weight) {
    if (!autoTareEnabled) return;
    
    unsigned long currentTime = millis();
    float weightChange = abs(weight - lastWeight);
    
    // Detect significant weight change (more than 5g) - cup placed
    if (weightChange > 5.0 && !waitingForStabilization) {
        waitingForStabilization = true;
        weightWhenChanged = weight;
        stabilizationStartTime = currentTime;
        lastWeightChangeTime = currentTime;
        Serial.println("Cup detected - waiting for weight to stabilize...");
    }
    
    // Check if weight is stabilizing
    if (waitingForStabilization) {
        float currentChange = abs(weight - weightWhenChanged);
        
        // If weight changed significantly from when we started waiting, reset timer
        if (currentChange > 1.0) { // Reduced from 2.0g to 1.0g for faster detection
            weightWhenChanged = weight;
            stabilizationStartTime = currentTime;
        }
        // If weight has been stable for 1 second, auto-tare (reduced from 2 seconds)
        else if (currentTime - stabilizationStartTime > 1000) {
            if (scalePtr != nullptr) {
                scalePtr->tare();
                Serial.println("Auto-tare executed - cup weight zeroed");
                showAutoTaredMessage(); // Use dedicated method
            }
            waitingForStabilization = false;
            autoTareEnabled = false; // Disable auto-tare, now ready for timer
            Serial.println("Auto-tare complete - auto-timer now enabled");
        }
    }
    
    lastWeight = weight;
}

void Display::checkAutoTimer(float flowRate) {
    // Only allow auto-timer to start if auto-tare has been completed
    // This prevents timer from starting when placing a cup on the scale
    if (!autoTareEnabled && !waitingForStabilization) {
        // Start timer when flow rate increases above threshold (0.2g/s)
        if (!timerRunning && !autoTimerStarted && flowRate > 0.2) {
            startTimer();
            autoTimerStarted = true;
            Serial.println("Auto-timer started - coffee flow detected");
        }
        
        // Pause timer immediately when flow rate drops to 0.0
        if (timerRunning && autoTimerStarted) {
            if (flowRate <= 0.0) {
                stopTimer();
                Serial.println("Auto-timer paused - coffee flow stopped (0.0g/s)");
            } else if (timerPaused && flowRate > 0.1) {
                // Resume timer if flow picks up again
                startTimer();
                Serial.println("Auto-timer resumed - coffee flow detected again");
            }
        }
    }
    
    lastFlowRate = flowRate;
}

void Display::resetAutoSequence() {
    if (currentMode == ScaleMode::AUTO) {
        autoTareEnabled = true;
        waitingForStabilization = false;
        autoTimerStarted = false;
        lastWeight = 0.0;
        lastFlowRate = 0.0;
        
        // Reset timer if it was auto-started
        if (timerRunning && autoTimerStarted) {
            resetTimer();
        }
        
        Serial.println("Auto sequence reset - ready for new auto-tare cycle");
    }
}
