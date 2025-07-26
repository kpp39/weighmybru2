#pragma once

#define FLOWRATE_AVG_WINDOW 12  // Reduced for faster response

class FlowRate {
public:
    FlowRate();
    void update(float currentWeight);
    float getFlowRate() const; // grams per second
private:
    float lastWeight;
    unsigned long lastTime;
    float flowRate;
    float flowRateBuffer[FLOWRATE_AVG_WINDOW];
    int bufferIndex;
    int bufferCount;
    
    // Flow rate filtering parameters
    static constexpr float WEIGHT_DEADBAND = 0.05f;     // Ignore changes smaller than 0.05g
    static constexpr float MIN_DELTA_TIME = 0.08f;      // Increased to 80ms for more stable readings
    static constexpr float ZERO_THRESHOLD = 0.05f;      // Clamp to zero if flow rate < 0.05 g/s
    static constexpr float RAPID_CHANGE_THRESHOLD = 1.0f; // Increased to 1.0g - only for major changes
    static constexpr float NEGATIVE_CHANGE_THRESHOLD = 0.3f; // Lower threshold for weight removal detection
    
    // Helper methods
    float calculateAdaptiveAverage(bool rapidChange);
};
