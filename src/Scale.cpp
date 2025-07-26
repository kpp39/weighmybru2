#include "Scale.h"
#include "WebServer.h"
#include "Calibration.h"

Scale::Scale(uint8_t dataPin, uint8_t clockPin, float calibrationFactor)
    : dataPin(dataPin), clockPin(clockPin), calibrationFactor(calibrationFactor), currentWeight(0.0f),
      readingIndex(0), lastStableWeight(0), lastSignificantChange(0), brewingActive(false), samplesInitialized(false),
      medianSamples(3), averageSamples(5) {
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
    float rawReading = hx711.get_units(1);
    
    // Handle NaN or invalid readings
    if (isnan(rawReading)) {
        return currentWeight; // Return last known good weight
    }
    
    // Initialize sample buffer on first valid reading
    if (!samplesInitialized) {
        initializeSamples(rawReading);
        currentWeight = rawReading;
        lastStableWeight = rawReading;
        return currentWeight;
    }
    
    // Detect if we're actively brewing (significant weight change)
    float weightChange = abs(rawReading - lastStableWeight);
    if (weightChange > brewingThreshold) {
        brewingActive = true;
        lastSignificantChange = millis();
    } else if (millis() - lastSignificantChange > stabilityTimeout) {
        brewingActive = false;
        lastStableWeight = rawReading;
    }
    
    // Store reading in circular buffer
    readings[readingIndex] = rawReading;
    readingIndex = (readingIndex + 1) % MAX_SAMPLES;
    
    float filteredWeight;
    if (brewingActive) {
        // During brewing: Use median filter (fast + noise resistant)
        filteredWeight = medianFilter(medianSamples);
    } else {
        // When stable: Use average filter (smooth + stable)
        filteredWeight = averageFilter(averageSamples);
    }
    
    currentWeight = filteredWeight;
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
    for (int i = 0; i < samples; i++) {
        int idx = (readingIndex - 1 - i + MAX_SAMPLES) % MAX_SAMPLES;
        sum += readings[idx];
    }
    return sum / samples;
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
    averageSamples = preferences.getInt("avg_samples", 5);
}
