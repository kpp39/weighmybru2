#pragma once

#define FLOWRATE_AVG_WINDOW 20  // Increased for better smoothing

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
    static constexpr float WEIGHT_DEADBAND = 0.08f;     // Increased deadband for load cell noise
    static constexpr float MIN_DELTA_TIME = 0.15f;      // Increased to 150ms for much more stable readings
    static constexpr float ZERO_THRESHOLD = 0.08f;      // Increased zero threshold
    static constexpr float RAPID_CHANGE_THRESHOLD = 1.5f; // Higher threshold for major changes
    static constexpr float NEGATIVE_CHANGE_THRESHOLD = 0.5f; // Higher threshold for weight removal
    
    // Helper methods
    float calculateStableAverage(bool isWeightRemoval);
};
