#include "FlowRate.h"
#include "Calibration.h"
#include <Arduino.h>

FlowRate::FlowRate() : lastWeight(0), lastTime(0), flowRate(0), bufferIndex(0), bufferCount(0) {
    for (int i = 0; i < FLOWRATE_AVG_WINDOW; ++i) flowRateBuffer[i] = 0;
}

void FlowRate::update(float currentWeight) {
    unsigned long now = millis();
    
    if (lastTime > 0) {
        float deltaWeight = currentWeight - lastWeight;
        float deltaTime = (now - lastTime) / 1000.0f; // seconds
        
        // Only update if enough time has passed for meaningful calculation
        if (deltaTime >= MIN_DELTA_TIME) {
            // Detect different types of changes with higher thresholds
            bool majorWeightIncrease = deltaWeight > RAPID_CHANGE_THRESHOLD;
            bool weightRemoval = deltaWeight < -NEGATIVE_CHANGE_THRESHOLD;
            
            // Apply stronger deadband filter for load cell noise
            if (abs(deltaWeight) < WEIGHT_DEADBAND) {
                deltaWeight = 0.0f;
            }
            
            if (deltaTime > 0) {
                float instantRate = deltaWeight / deltaTime;
                
                // Only clear buffer for significant weight removal
                if (weightRemoval && abs(deltaWeight) > 1.0f) {
                    // Major weight removed - reset buffer for fast zero response
                    for (int i = 0; i < FLOWRATE_AVG_WINDOW; i++) {
                        flowRateBuffer[i] = 0.0f;
                    }
                    bufferCount = 1;
                    bufferIndex = 0;
                    flowRateBuffer[0] = instantRate;
                } else {
                    // Normal operation - store in circular buffer
                    flowRateBuffer[bufferIndex] = instantRate;
                    bufferIndex = (bufferIndex + 1) % FLOWRATE_AVG_WINDOW;
                    if (bufferCount < FLOWRATE_AVG_WINDOW) bufferCount++;
                }
                
                // Calculate stable average with consistent smoothing
                flowRate = calculateStableAverage(weightRemoval);
                
                // Apply zero threshold to eliminate tiny fluctuations
                if (abs(flowRate) < ZERO_THRESHOLD) {
                    flowRate = 0.0f;
                }
            }
            
            lastWeight = currentWeight;
            lastTime = now;
        }
    } else {
        // First update - just store the values
        lastWeight = currentWeight;
        lastTime = now;
    }
}

float FlowRate::calculateStableAverage(bool isWeightRemoval) {
    if (bufferCount == 0) return 0.0f;
    
    if (isWeightRemoval) {
        // For weight removal, use fewer samples for faster zero response
        int samplesToUse = min(5, bufferCount);
        float sum = 0.0f;
        for (int i = 0; i < samplesToUse; i++) {
            int index = (bufferIndex - 1 - i + FLOWRATE_AVG_WINDOW) % FLOWRATE_AVG_WINDOW;
            sum += flowRateBuffer[index];
        }
        return sum / samplesToUse;
    } else {
        // Normal operation - use gentle linear weighting for maximum stability
        float weightedSum = 0.0f;
        float totalWeight = 0.0f;
        
        // Use more samples with gentle linear weighting (not exponential)
        int samplesToUse = min(bufferCount, FLOWRATE_AVG_WINDOW);
        for (int i = 0; i < samplesToUse; i++) {
            int index = (bufferIndex - 1 - i + FLOWRATE_AVG_WINDOW) % FLOWRATE_AVG_WINDOW;
            // Gentle linear weighting: recent samples slightly more important
            float weight = 1.0f + (0.05f * (samplesToUse - i)); // Very gentle weighting
            weightedSum += flowRateBuffer[index] * weight;
            totalWeight += weight;
        }
        
        return weightedSum / totalWeight;
    }
}

float FlowRate::getFlowRate() const {
    return flowRate;
}
