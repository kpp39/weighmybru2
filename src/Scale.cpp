#include "Scale.h"
#include "WebServer.h"
#include "Calibration.h"

Scale::Scale(uint8_t dataPin, uint8_t clockPin, float calibrationFactor)
    : dataPin(dataPin), clockPin(clockPin), calibrationFactor(calibrationFactor), currentWeight(0.0f),
      readingIndex(0), samplesInitialized(false), previousFilteredWeight(0), medianSamples(3), averageSamples(2) {
    // Initialize readings array
    for (int i = 0; i < MAX_SAMPLES; i++) {
        readings[i] = 0.0f;
    }
}

bool Scale::begin() {
    Serial.println("Starting scale initialization...");
    
    preferences.begin("scale", false);
    calibrationFactor = preferences.getFloat("calib", calibrationFactor);
    
    // Load filtering parameters with load cell-specific defaults
    loadFilterSettings();
    
    // Auto-adjust brewing threshold based on calibration factor and load cell characteristics
    // Only if not previously saved by user (check if key exists)
    if (!preferences.isKey("brew_thresh")) {
        // For 3kg load cells (1mV/V): calibration factors typically 400-800
        // For 500g load cells (2mV/V): calibration factors typically 2000-5000+
        if (calibrationFactor < 1000) {
            brewingThreshold = 0.25f;  // 3kg load cell with 1mV/V - needs higher threshold due to lower sensitivity
            Serial.println("Auto-detected 3kg load cell (low calibration factor)");
        } else if (calibrationFactor < 2500) {
            brewingThreshold = 0.15f; // Medium sensitivity load cell
            Serial.println("Auto-detected medium sensitivity load cell");
        } else {
            brewingThreshold = 0.1f;  // 500g load cell with 2mV/V - more sensitive, can use lower threshold
            Serial.println("Auto-detected high sensitivity load cell (500g/2mV/V type)");
        }
        saveFilterSettings(); // Save auto-detected values
    }
    
    preferences.end();
    
    // Initialize HX711 with extremely conservative approach
    Serial.println("Initializing HX711...");
    
    // First verify pins are valid and accessible
    if (dataPin > 48 || clockPin > 48) {  // ESP32-S3 has max 48 GPIO pins
        Serial.println("ERROR: Invalid pin numbers");
        isConnected = false;
        return false;
    }
    
    // Test pin accessibility without configuring them yet
    Serial.println("Testing pin accessibility...");
    pinMode(dataPin, INPUT);
    pinMode(clockPin, OUTPUT);
    
    // Initialize clock pin to LOW
    digitalWrite(clockPin, LOW);
    delay(100);  // Give HX711 time to respond
    
    // Check data pin state - HX711 may be LOW when busy, so this is just informational
    bool dataLineState = digitalRead(dataPin);
    Serial.println("Data pin state: " + String(dataLineState ? "HIGH" : "LOW"));
    
    // Try to initialize HX711 regardless of initial data pin state
    Serial.println("Attempting HX711 initialization...");
    
    // Initialize HX711 object
    hx711.begin(dataPin, clockPin);
    
    // Give HX711 time to stabilize after power-on
    delay(500);
    
    // Test if HX711 responds within reasonable time
    Serial.println("Testing HX711 responsiveness...");
    unsigned long startTime = millis();
    bool hx711Ready = false;
    
    // Try for up to 3 seconds to get a response
    while (millis() - startTime < 3000) {
        if (hx711.is_ready()) {
            hx711Ready = true;
            Serial.println("HX711 is ready after " + String(millis() - startTime) + "ms");
            break;
        }
        delay(50);
    }
    
    if (!hx711Ready) {
        Serial.println("HX711 not responding after 3 seconds - checking for presence");
        
        // Try to wake up HX711 by pulsing clock
        for (int i = 0; i < 25; i++) {  // 25 pulses to reset
            digitalWrite(clockPin, HIGH);
            delayMicroseconds(1);
            digitalWrite(clockPin, LOW);
            delayMicroseconds(1);
        }
        delay(100);
        
        // Try one more time
        for (int i = 0; i < 10; i++) {
            if (hx711.is_ready()) {
                hx711Ready = true;
                Serial.println("HX711 ready after reset pulse");
                break;
            }
            delay(100);
        }
    }
    
    if (!hx711Ready) {
        Serial.println("HX711 still not responding - may not be connected or powered");
        isConnected = false;
        return false;
    }
    
    // Set connected flag and configure scale
    isConnected = true;
    
    // Set scale factor
    hx711.set_scale(calibrationFactor);
    
    // Try to get an initial reading to verify everything is working
    Serial.println("Getting initial reading to verify HX711 operation...");
    float testReading = 0;
    bool gotReading = false;
    
    for (int i = 0; i < 5; i++) {
        if (hx711.is_ready()) {
            testReading = hx711.get_units(1);
            if (!isnan(testReading)) {
                gotReading = true;
                break;
            }
        }
        delay(200);
    }
    
    if (gotReading) {
        Serial.println("Initial reading: " + String(testReading) + "g");
        Serial.println("HX711 is functioning correctly");
        
        // Auto-tare the scale during initialization for a clean start
        Serial.println("Auto-taring scale for clean startup...");
        hx711.tare(5);  // Take 5 readings for accurate tare
        
        // Verify tare was successful
        delay(500);  // Let it settle
        float postTareReading = 0;
        for (int i = 0; i < 3; i++) {
            if (hx711.is_ready()) {
                postTareReading = hx711.get_units(1);
                if (!isnan(postTareReading)) {
                    break;
                }
            }
            delay(100);
        }
        
        Serial.println("Reading after tare: " + String(postTareReading) + "g");
        
        if (abs(postTareReading) < 1.0) {  // Should be close to 0 after tare
            Serial.println("Auto-tare successful - scale zeroed");
        } else {
            Serial.println("WARNING: Large offset after tare - check for load on scale");
        }
        
    } else {
        Serial.println("WARNING: Could not get valid reading, but HX711 appears connected");
    }
    
    Serial.println("HX711 initialized successfully");
    Serial.println("Using pins - Data: GPIO" + String(dataPin) + ", Clock: GPIO" + String(clockPin));
    Serial.println("Calibration factor: " + String(calibrationFactor));
    Serial.println("Scale filtering configured:");
    Serial.println("Brewing threshold: " + String(brewingThreshold) + "g");
    Serial.println("Stability timeout: " + String(stabilityTimeout) + "ms");
    Serial.println("Median samples: " + String(medianSamples));
    Serial.println("Average samples: " + String(averageSamples));
    
    return true;
}

void Scale::tare(uint8_t times) {
    if (!isConnected) {
        Serial.println("Cannot tare: HX711 not connected");
        return;
    }
    
    Serial.println("Taring scale with " + String(times) + " readings...");
    
    // Clear any existing filter data before taring
    samplesInitialized = false;
    for (int i = 0; i < MAX_SAMPLES; i++) {
        readings[i] = 0.0f;
    }
    readingIndex = 0;
    
    // Perform the tare
    hx711.tare(times);
    
    // Reset current weight to 0
    currentWeight = 0.0f;
    
    Serial.println("Tare complete - scale zeroed");
}

void Scale::set_scale(float factor) {
    // Only save if the calibration factor actually changed
    if (calibrationFactor != factor) {
        calibrationFactor = factor;
        hx711.set_scale(calibrationFactor);
        saveCalibration();
    }
}

void Scale::saveCalibration() {
    preferences.begin("scale", false);
    preferences.putFloat("calib", calibrationFactor);
    preferences.end();
}

void Scale::loadCalibration() {
    preferences.begin("scale", true);
    calibrationFactor = preferences.getFloat("calib", calibrationFactor);
    preferences.end();
}

float Scale::getWeight() {
    // Return 0 if HX711 is not connected
    if (!isConnected) {
        return 0.0f;
    }
    
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    
    // Read at 50Hz (every 20ms) for good responsiveness
    if (currentTime - lastReadTime < 20) {
        return currentWeight;
    }
    lastReadTime = currentTime;

    // Check if HX711 is ready before attempting to read
    if (!hx711.is_ready()) {
        return currentWeight;  // Return last known value if not ready
    }
    
    float rawReading = hx711.get_units(1);
    
    // Handle NaN or invalid readings
    if (isnan(rawReading)) {
        return currentWeight;
    }
    
    // Initialize sample buffer on first valid reading
    if (!samplesInitialized) {
        initializeSamples(rawReading);
        currentWeight = rawReading;
        return currentWeight;
    }
    
    // Store reading in circular buffer
    readings[readingIndex] = rawReading;
    readingIndex = (readingIndex + 1) % MAX_SAMPLES;
    
    // Simple average filter for stability
    currentWeight = averageFilter(averageSamples);
    return currentWeight;
}

float Scale::getCurrentWeight() {
    return currentWeight;
}

long Scale::getRawValue() {
    if (!isConnected) {
        return 0;  // Return 0 if HX711 not connected
    }
    return hx711.get_value(1); // Get raw value from HX711
}

void Scale::initializeSamples(float initialValue) {
    for (int i = 0; i < MAX_SAMPLES; i++) {
        readings[i] = initialValue;
    }
    samplesInitialized = true;
}

float Scale::medianFilter(int samples) {
    if (samples > MAX_SAMPLES) samples = MAX_SAMPLES;
    
    // Copy recent readings
    float temp[samples];
    for (int i = 0; i < samples; i++) {
        int idx = (readingIndex - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
        temp[i] = readings[idx];
    }
    
    // Simple bubble sort for median (efficient for small arrays)
    for (int i = 0; i < samples - 1; i++) {
        for (int j = 0; j < samples - i - 1; j++) {
            if (temp[j] > temp[j + 1]) {
                float swap = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = swap;
            }
        }
    }
    return temp[samples / 2]; // Return median
}

float Scale::averageFilter(int samples) {
    if (samples > MAX_SAMPLES) samples = MAX_SAMPLES;
    
    float sum = 0;
    int validSamples = 0;
    
    // Calculate average of recent samples
    for (int i = 0; i < samples; i++) {
        int idx = (readingIndex - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
        sum += readings[idx];
        validSamples++;
    }
    
    return sum / validSamples; // Return simple average without additional smoothing
}

// Filter parameter setters with validation
void Scale::setBrewingThreshold(float threshold) {
    if (threshold >= 0.05f && threshold <= 1.0f) { // Reasonable bounds
        brewingThreshold = threshold;
        saveFilterSettings();
    }
}

void Scale::setStabilityTimeout(unsigned long timeout) {
    if (timeout >= 500 && timeout <= 10000) { // 0.5-10 seconds
        stabilityTimeout = timeout;
        saveFilterSettings();
    }
}

void Scale::setMedianSamples(int samples) {
    if (samples >= 1 && samples <= MAX_SAMPLES) {
        medianSamples = samples;
        saveFilterSettings();
    }
}

void Scale::setAverageSamples(int samples) {
    if (samples >= 1 && samples <= MAX_SAMPLES) {
        averageSamples = samples;
        saveFilterSettings();
    }
}

void Scale::saveFilterSettings() {
    preferences.begin("scale", false);
    preferences.putFloat("brew_thresh", brewingThreshold);
    preferences.putULong("stab_timeout", stabilityTimeout);
    preferences.putInt("median_samples", medianSamples);
    preferences.putInt("avg_samples", averageSamples);
    preferences.end();
    Serial.println("Filter settings saved to EEPROM");
}

void Scale::loadFilterSettings() {
    // Load with sensible defaults
    brewingThreshold = preferences.getFloat("brew_thresh", 0.15f);
    stabilityTimeout = preferences.getULong("stab_timeout", 2000);
    medianSamples = preferences.getInt("median_samples", 3);
    averageSamples = preferences.getInt("avg_samples", 2); // Reduced for faster response
}
