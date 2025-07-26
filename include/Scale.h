#ifndef SCALE_H
#define SCALE_H

#include <HX711.h>
#include <Preferences.h>

class Scale {
public:
    Scale(uint8_t dataPin, uint8_t clockPin, float calibrationFactor);
    void begin();
    void tare(uint8_t times = 20);
    void set_scale(float factor);
    float getWeight();
    float getCurrentWeight();
    long getRawValue();
    void saveCalibration(); // Save calibration factor to NVS
    void loadCalibration(); // Load calibration factor from NVS
    float getCalibrationFactor() const { return calibrationFactor; } // Getter for API
    
    // Filtering configuration - adjustable for different load cells
    void setBrewingThreshold(float threshold);
    void setStabilityTimeout(unsigned long timeout);
    void setMedianSamples(int samples);
    void setAverageSamples(int samples);
    
    float getBrewingThreshold() const { return brewingThreshold; }
    unsigned long getStabilityTimeout() const { return stabilityTimeout; }
    int getMedianSamples() const { return medianSamples; }
    int getAverageSamples() const { return averageSamples; }
    
    void saveFilterSettings();
    void loadFilterSettings();
    
private:
    HX711 hx711;
    Preferences preferences;
    uint8_t dataPin;
    uint8_t clockPin;
    float calibrationFactor = 0.0f;
    float currentWeight;
    
    // Smart filtering variables
    static const int MAX_SAMPLES = 20;
    float readings[MAX_SAMPLES];
    int readingIndex = 0;
    float lastStableWeight = 0;
    unsigned long lastSignificantChange = 0;
    bool brewingActive = false;
    bool samplesInitialized = false;
    
    // Configurable filtering parameters
    float brewingThreshold = 0.15f;  // Adjustable based on load cell sensitivity
    unsigned long stabilityTimeout = 2000;  // 2 seconds default
    int medianSamples = 3;  // Samples for brewing mode (median filter)
    int averageSamples = 5;  // Samples for stable mode (average filter)
    
    // Filter methods
    float medianFilter(int samples);
    float averageFilter(int samples);
    void initializeSamples(float initialValue);
};

#endif
