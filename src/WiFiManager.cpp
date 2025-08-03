#include "WiFiManager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>

Preferences wifiPrefs;

// Station credentials
char stored_ssid[33] = {0};
char stored_password[65] = {0};

// Cache for WiFi credentials to avoid repeated slow EEPROM reads
static String cachedSSID = "";
static String cachedPassword = "";
static bool credentialsCached = false;
static unsigned long lastCacheTime = 0;
const unsigned long CACHE_TIMEOUT = 300000; // 5 minutes cache timeout

// AP credentials
const char* ap_ssid = "WeighMyBru-AP";
const char* ap_password = "";

unsigned long startAttemptTime = 0;
const unsigned long timeout = 10000; // 10 seconds

void saveWiFiCredentials(const char* ssid, const char* password) {
    Serial.println("Saving WiFi credentials...");
    unsigned long startTime = millis();
    
    if (wifiPrefs.begin("wifi", false)) {
        wifiPrefs.putString("ssid", ssid);
        wifiPrefs.putString("password", password);
        wifiPrefs.end();
        
        // Update cache immediately
        cachedSSID = String(ssid);
        cachedPassword = String(password);
        credentialsCached = true;
        lastCacheTime = millis();
        
        Serial.printf("WiFi credentials saved in %lu ms\n", millis() - startTime);
    } else {
        Serial.println("ERROR: Failed to open WiFi preferences for writing");
    }
}

void clearWiFiCredentials() {
    Serial.println("Clearing WiFi credentials...");
    if (wifiPrefs.begin("wifi", false)) {
        wifiPrefs.clear();
        wifiPrefs.end();
        
        // Clear cache
        cachedSSID = "";
        cachedPassword = "";
        credentialsCached = true;
        lastCacheTime = millis();
        
        Serial.println("WiFi credentials cleared");
    } else {
        Serial.println("ERROR: Failed to open WiFi preferences for clearing");
    }
}

bool loadWiFiCredentialsFromEEPROM() {
    // Check if cache is still valid first (no serial output for speed)
    if (credentialsCached && (millis() - lastCacheTime < CACHE_TIMEOUT)) {
        return true;
    }
    
    unsigned long startTime = millis();
    
    // Add timeout protection
    const unsigned long EEPROM_TIMEOUT = 5000; // 5 second timeout
    bool success = false;
    
    unsigned long attemptStart = millis();
    if (wifiPrefs.begin("wifi", true)) {
        // Check if operation is taking too long
        if (millis() - attemptStart > EEPROM_TIMEOUT) {
            wifiPrefs.end();
            cachedSSID = "";
            cachedPassword = "";
        } else {
            cachedSSID = wifiPrefs.getString("ssid", "");
            cachedPassword = wifiPrefs.getString("password", "");
            success = true;
        }
        wifiPrefs.end();
        
        credentialsCached = true;
        lastCacheTime = millis();
        
        // Minimal serial output to reduce blocking
        Serial.printf("WiFi: %s in %lums\n", success ? "OK" : "TIMEOUT", millis() - startTime);
        return success;
    } else {
        // Use empty defaults if EEPROM fails
        cachedSSID = "";
        cachedPassword = "";
        credentialsCached = true;
        lastCacheTime = millis();
        Serial.println("WiFi: EEPROM FAIL");
        return false;
    }
}

void loadWiFiCredentials(char* ssid, char* password, size_t maxLen) {
    loadWiFiCredentialsFromEEPROM();
    strncpy(ssid, cachedSSID.c_str(), maxLen - 1);
    strncpy(password, cachedPassword.c_str(), maxLen - 1);
    ssid[maxLen - 1] = '\0';
    password[maxLen - 1] = '\0';
}

String getStoredSSID() {
    // Fast path - return immediately if already cached and recent
    if (credentialsCached && (millis() - lastCacheTime < CACHE_TIMEOUT)) {
        return cachedSSID;
    }
    
    // Only do EEPROM read if cache is invalid
    loadWiFiCredentialsFromEEPROM();
    return cachedSSID;
}

String getStoredPassword() {
    // Fast path - return immediately if already cached and recent
    if (credentialsCached && (millis() - lastCacheTime < CACHE_TIMEOUT)) {
        return cachedPassword;
    }
    
    // Only do EEPROM read if cache is invalid
    loadWiFiCredentialsFromEEPROM();
    return cachedPassword;
}

void setupWiFi() {
    char ssid[33] = {0};
    char password[65] = {0};
    loadWiFiCredentials(ssid, password, sizeof(ssid));
    
    // Ensure WiFi is completely reset first
    Serial.println("Resetting WiFi...");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    
    // Always start with AP mode first for stable operation
    Serial.println("Starting AP mode...");
    WiFi.mode(WIFI_AP); // Start with AP only first
    
    // Configure AP with specific settings for better visibility
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    bool apStarted = WiFi.softAP(ap_ssid, ap_password, 6, 0, 4); // Channel 6 (less congested), no hidden, max 4 connections
    
    Serial.println("AP Started: " + String(apStarted ? "YES" : "NO"));
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    Serial.println("AP SSID: " + String(ap_ssid));
    Serial.println("AP MAC: " + WiFi.softAPmacAddress());
    
    // Longer delay to ensure AP is fully stable before attempting STA
    delay(3000);
    
    // Only try STA connection if credentials exist
    if (strlen(ssid) > 0) {
        Serial.println("Attempting STA connection to: " + String(ssid));
        
        // Switch to dual mode only after AP is stable
        WiFi.mode(WIFI_AP_STA);
        delay(1000); // Give time for mode switch
        
        startAttemptTime = millis();
        WiFi.begin(ssid, password);
        
        // Wait for connection with shorter timeout to avoid AP disruption
        int connectionAttempts = 0;
        const int maxAttempts = 20; // 10 seconds max
        
        while (WiFi.status() != WL_CONNECTED && connectionAttempts < maxAttempts) {
            delay(500);
            Serial.print(".");
            connectionAttempts++;
            
            // Check if connection is failing badly
            if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED) {
                Serial.println("\nSTA connection failed - bad credentials detected");
                Serial.println("Clearing stored credentials and keeping AP-only mode");
                
                // Clear bad credentials
                clearWiFiCredentials();
                
                // Disconnect STA and go back to AP-only mode for stability
                WiFi.disconnect(true);
                WiFi.mode(WIFI_AP);
                delay(1000);
                
                // Restart AP to ensure it's stable
                WiFi.softAP(ap_ssid, ap_password, 6, 0, 4);
                Serial.println("AP mode restored after credential failure");
                break;
            }
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected to STA with IP: " + WiFi.localIP().toString());
            Serial.println("Running in dual AP+STA mode");
        } else {
            Serial.println("\nSTA connection timeout. Running AP-only mode for stability.");
            // Ensure we're back in AP-only mode
            WiFi.disconnect(true);
            WiFi.mode(WIFI_AP);
            delay(1000);
            WiFi.softAP(ap_ssid, ap_password, 6, 0, 4);
        }
    } else {
        Serial.println("No stored WiFi credentials. AP mode only.");
    }
    
    // Setup mDNS after WiFi is stable
    setupmDNS();
    
    Serial.println("WiFi setup complete. AP mode: " + String(ap_ssid) + " is always available.");
}

void setupmDNS() {
    // Start mDNS service with hostname "weighmybru"
    if (MDNS.begin("weighmybru")) {
        Serial.println("mDNS responder started");
        Serial.println("Access the scale at: http://weighmybru.local");
        
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("websocket", "tcp", 81);
        
        // Add some useful service properties
        MDNS.addServiceTxt("http", "tcp", "device", "WeighMyBru Coffee Scale");
        MDNS.addServiceTxt("http", "tcp", "version", "2.0");
        
    } else {
        Serial.println("Error starting mDNS responder");
    }
}

void printWiFiStatus() {
    Serial.println("=== WiFi Status ===");
    Serial.println("WiFi Mode: " + String(WiFi.getMode()));
    Serial.println("AP Status: " + String(WiFi.softAPgetStationNum()) + " clients connected");
    Serial.println("AP IP: " + WiFi.softAPIP().toString());
    Serial.println("AP SSID: " + String(ap_ssid));
    Serial.println("STA Status: " + String(WiFi.status()));
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("STA IP: " + WiFi.localIP().toString());
        Serial.println("STA RSSI: " + String(WiFi.RSSI()) + " dBm");
    }
    Serial.println("==================");
}
