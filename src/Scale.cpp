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

void Scale::begin() {
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
    hx711.begin(dataPin, clockPin);
    hx711.set_scale(calibrationFactor);
    tare();
    
    Serial.println("Scale filtering configured:");
    Serial.println("Brewing threshold: " + String(brewingThreshold) + "g");
    Serial.println("Stability timeout: " + String(stabilityTimeout) + "ms");
    Serial.println("Median samples: " + String(medianSamples));
    Serial.println("Average samples: " + String(averageSamples));
}

void Scale::tare(uint8_t times) {
    hx711.tare(times);
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
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    
    // Read at 50Hz (every 20ms) for good responsiveness
    if (currentTime - lastReadTime < 20) {
        return currentWeight;
    }
    lastReadTime = currentTime;
    
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
