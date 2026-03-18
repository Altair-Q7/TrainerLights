// TrainerLights Server
// Author: khader afeez
// khaderafeez16@gmail.com

#include <ESP8266WiFi.h>
#include <TaskScheduler.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <EEPROM.h>

extern "C" {
#include "user_interface.h"
}

// ========== EEPROM Configuration ==========
#define EEPROM_SIZE 64
#define CONFIG_ADDR 0
#define MAGIC_NUMBER 0xBCDA

// ========== Pin Definitions ==========
#define LED_STATUS          D4   // Built-in LED
#define LED_STIMULUS        D6   // Stimulus LED
#define LED_ERROR           D7   // Red error LED
#define LED_SUCCESS         D0   // Green success LED
#define BUTTON_PIN          D5   // Multi-purpose button
#define TRIGGER_PIN         D1   // Ultrasonic trigger
#define ECHO_PIN            D2   // Ultrasonic echo

// ========== Client Configuration Structure ==========
struct ClientConfig {
    uint16_t magic;
    char device_name[20];
    int calibration_offset;
    uint16_t checksum;
};

// ========== Global Variables ==========
bool webSocketConnected = false;
WebSocketsClient webSocket;
String responseMessage;

// Configuration from server
int timeoutDuration = 2000;
int preDelay = 0;
int minDetectionRange = 0;
int maxDetectionRange = 50;

// Client state
unsigned long stimulusStartTime = 0;
unsigned long currentTime = 0;
bool isStimulating = false;
bool isCalibrating = false;
uint8_t connectionAttempts = 0;
unsigned long lastHeartbeat = 0;
int confirmCount = 0;  // Moved here to be global

// Distance measurement
long duration, distance, lastDistance;
float distanceBuffer[5] = {0};
int bufferIndex = 0;

// Device identification
String deviceId;
String deviceIP;

// ========== Task Scheduler ==========
Scheduler taskScheduler;

// Function declarations
void MeasureDistance();
void StimulusTimeout();
void StimulusStart();
void TurnOffLeds();
void SendHeartbeat();
void CheckButton();
void CalibrateSensor();

// Tasks
Task tMeasureDistance(30, TASK_FOREVER, &MeasureDistance, &taskScheduler, true);
Task tStimulusStart(0, TASK_ONCE, &StimulusStart, &taskScheduler, false);
Task tStimulusTimeout(10000, TASK_ONCE, &StimulusTimeout, &taskScheduler, false);
Task tTurnOffLeds(100, TASK_ONCE, &TurnOffLeds, &taskScheduler, false);
Task tHeartbeat(10000, TASK_FOREVER, &SendHeartbeat, &taskScheduler, true);
Task tCheckButton(100, TASK_FOREVER, &CheckButton, &taskScheduler, true);

// ========== WiFi Configuration ==========
const char* ssid = "TrainerLights";
const char* password = "1234567890";
const char* serverHost = "192.168.4.1";
const int serverPort = 81;

// ========== EEPROM Functions ==========
uint16_t calculateChecksum(ClientConfig &cfg) {
    uint16_t sum = 0;
    uint8_t* ptr = (uint8_t*)&cfg;
    for(int i = sizeof(cfg.magic); i < sizeof(ClientConfig) - sizeof(cfg.checksum); i++) {
        sum += ptr[i];
    }
    return sum;
}

void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    ClientConfig cfg;
    EEPROM.get(CONFIG_ADDR, cfg);
    
    if (cfg.magic == MAGIC_NUMBER && cfg.checksum == calculateChecksum(cfg)) {
        Serial.printf("✅ Loaded config: %s, cal: %d\n", cfg.device_name, cfg.calibration_offset);
    } else {
        ClientConfig defaultCfg;
        defaultCfg.magic = MAGIC_NUMBER;
        
        uint8_t mac[6];
        WiFi.macAddress(mac);
        sprintf(defaultCfg.device_name, "Light-%02X%02X", mac[4], mac[5]);
        
        defaultCfg.calibration_offset = 0;
        defaultCfg.checksum = calculateChecksum(defaultCfg);
        
        EEPROM.put(CONFIG_ADDR, defaultCfg);
        EEPROM.commit();
        Serial.printf("⚙️ Created default config: %s\n", defaultCfg.device_name);
    }
    EEPROM.end();
}

// ========== Generate Unique Device ID ==========
String generateDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[18];
    sprintf(id, "%02X:%02X:%02X:%02X:%02X:%02X", 
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(id);
}

// ========== WebSocket Event Handler ==========
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("🔴 WebSocket disconnected\n");
            webSocketConnected = false;
            connectionAttempts++;
            
            for(int i=0; i<3; i++) {
                digitalWrite(LED_ERROR, HIGH);
                delay(100);
                digitalWrite(LED_ERROR, LOW);
                delay(100);
            }
            break;
            
        case WStype_CONNECTED: {
            Serial.printf("🟢 WebSocket connected\n");
            webSocketConnected = true;
            connectionAttempts = 0;
            
            deviceIP = WiFi.localIP().toString();
            
            // Create registration message
            String regMsg = "{\"type\":\"sensor\",\"ip\":\"" + deviceIP + "\",\"mac\":\"" + deviceId + "\",\"name\":\"Light\"}";
            
            webSocket.sendTXT(regMsg);
            
            Serial.printf("📡 Registered: %s (%s)\n", deviceIP.c_str(), deviceId.c_str());
            
            digitalWrite(LED_SUCCESS, HIGH);
            delay(200);
            digitalWrite(LED_SUCCESS, LOW);
            break;
        }
        
        case WStype_TEXT: {
            Serial.printf("📨 Received: %s\n", payload);

            StaticJsonDocument<512> root;
            auto error = deserializeJson(root, payload);
            
            if (error) {
                Serial.print("❌ JSON parsing failed: ");
                Serial.println(error.c_str());
                break;
            }
            
            const char* messageType = root["type"];
            
            // ===== Stimulus Command =====
            if (String(messageType) == String("stimulus")) {
                Serial.println("⚡ STIMULUS COMMAND");
                timeoutDuration = root["timeout"] | 2000;
                preDelay = root["delay"] | 0;
                minDetectionRange = root["min_detection_range"] | 0;
                maxDetectionRange = root["max_detection_range"] | 50;
                
                Serial.printf("   Delay: %dms, Timeout: %dms, Range: %d-%dcm\n", 
                             preDelay, timeoutDuration, minDetectionRange, maxDetectionRange);
                
                tStimulusStart.setInterval(preDelay);
                tStimulusStart.restartDelayed();
            }
            
            // ===== Ping/Heartbeat =====
            if (String(messageType) == String("ping")) {
                webSocket.sendTXT("{\"type\":\"pong\"}");
                lastHeartbeat = millis();
            }
            
            // ===== Calibration Command =====
            if (String(messageType) == String("calibrate")) {
                Serial.println("🔧 Calibration command received");
                isCalibrating = true;
                digitalWrite(LED_STIMULUS, HIGH);
            }
            
            // ===== Restart Command =====
            if (String(messageType) == String("restart")) {
                Serial.println("🔄 Restart command received");
                delay(100);
                ESP.restart();
            }
            break;
        }
        
        case WStype_BIN:
            Serial.printf("📦 Received binary: %u bytes\n", length);
            break;
            
        case WStype_PING:
            Serial.println("📡 Ping received");
            break;
            
        case WStype_PONG:
            Serial.println("📡 Pong received");
            break;
            
        default:
            break;
    }
}

// ========== Send Heartbeat ==========
void SendHeartbeat() {
    if (webSocketConnected) {
        webSocket.sendTXT("{\"type\":\"heartbeat\"}");
        Serial.println("💓 Heartbeat sent");
    }
}

// ========== WiFi Connection with Retry ==========
void monitorWiFi() {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();
    
    if(WiFi.status() != WL_CONNECTED) {
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            
            Serial.printf("📡 Connecting to %s", ssid);
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, password);
            
            for(int i=0; i<20 && WiFi.status() != WL_CONNECTED; i++) {
                digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
                delay(250);
                Serial.print(".");
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("\n✅ WiFi connected");
                Serial.printf("   IP: %s\n", WiFi.localIP().toString().c_str());
                
                webSocket.begin(serverHost, serverPort, "/");
                webSocket.onEvent(webSocketEvent);
                webSocket.setReconnectInterval(2000);
            }
        }
    }
}

// ========== Check Button Press ==========
void CheckButton() {
    static bool lastButtonState = HIGH;
    static unsigned long pressStartTime = 0;
    
    bool buttonState = digitalRead(BUTTON_PIN);
    
    if (buttonState == LOW && lastButtonState == HIGH) {
        pressStartTime = millis();
        Serial.println("👆 Button pressed");
    }
    
    if (buttonState == HIGH && lastButtonState == LOW) {
        unsigned long pressDuration = millis() - pressStartTime;
        
        if (pressDuration < 1000) {
            digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
            Serial.println("🔦 Short press - toggled LED");
        } 
        else if (pressDuration < 3000) {
            Serial.println("🎯 Medium press - manual trigger");
            StimulusStart();
        }
        else {
            Serial.println("🔧 Long press - calibrating");
            CalibrateSensor();
        }
    }
    
    lastButtonState = buttonState;
}

// ========== Calibrate Sensor ==========
void CalibrateSensor() {
    Serial.println("📏 Calibrating distance sensor...");
    digitalWrite(LED_ERROR, HIGH);
    digitalWrite(LED_SUCCESS, HIGH);
    
    long total = 0;
    int samples = 10;
    
    for(int i=0; i<samples; i++) {
        digitalWrite(TRIGGER_PIN, LOW);  
        delayMicroseconds(2); 
        digitalWrite(TRIGGER_PIN, HIGH);
        delayMicroseconds(10); 
        digitalWrite(TRIGGER_PIN, LOW);
        
        duration = pulseIn(ECHO_PIN, HIGH, 30000);
        if (duration > 0) {
            total += (duration / 2) / 29.1;
        }
        delay(100);
    }
    
    long avgDistance = total / samples;
    Serial.printf("📊 Average distance: %ldcm\n", avgDistance);
    
    if (avgDistance > 100) {
        Serial.println("⚠️ Sensor may need adjustment");
    }
    
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_SUCCESS, LOW);
}

// ========== Improved Distance Measurement with Filtering ==========
float getFilteredDistance() {
    float sum = 0;
    for(int i=0; i<5; i++) {
        sum += distanceBuffer[i];
    }
    return sum / 5.0;
}

void MeasureDistance() {
    
    digitalWrite(TRIGGER_PIN, LOW);  
    delayMicroseconds(2); 
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10); 
    digitalWrite(TRIGGER_PIN, LOW);
    
    duration = pulseIn(ECHO_PIN, HIGH, 30000);
    
    if (duration == 0) {
        return;
    }
    
    float rawDistance = (duration / 2.0) / 29.1;
    
    distanceBuffer[bufferIndex] = rawDistance;
    bufferIndex = (bufferIndex + 1) % 5;
    
    float filteredDistance = getFilteredDistance();
    distance = (long)filteredDistance;

    static bool wasInRange = false;
    bool inRange = (distance <= maxDetectionRange && distance >= minDetectionRange);
    
    static int confirmCount = 0;
    
    if (inRange) {
        confirmCount++;
    } else {
        confirmCount = 0;
    }
    
    if (confirmCount >= 2 && isStimulating) {
        
        tStimulusTimeout.disable();
        currentTime = millis();
        unsigned long reactionTime = currentTime - stimulusStartTime;

        if (reactionTime > 50) {
            
            digitalWrite(LED_STIMULUS, LOW);
            digitalWrite(LED_SUCCESS, HIGH);
            tTurnOffLeds.setInterval(100);
            tTurnOffLeds.restartDelayed();
            
            isStimulating = false;
            
            String responseMsg = "{\"type\":\"response\",\"time\":\"" + String(reactionTime) + 
                                 "\",\"ip\":\"" + deviceIP + 
                                 "\",\"mac\":\"" + deviceId + 
                                 "\",\"distance\":\"" + String(distance) + 
                                 "\",\"error\":\"0\"}";
            
            webSocket.sendTXT(responseMsg);
            
            Serial.printf("✅ Hit! Time: %lums, Distance: %ldcm\n", reactionTime, distance);
        } else {
            StimulusTimeout();
        }
    }
}

// ========== Stimulus Timeout ==========
void StimulusTimeout() {
    
    digitalWrite(LED_STIMULUS, LOW);
    isStimulating = false;
    
    String responseMsg = "{\"type\":\"response\",\"time\":\"" + String(preDelay + timeoutDuration) + 
                         "\",\"ip\":\"" + deviceIP + 
                         "\",\"mac\":\"" + deviceId + 
                         "\",\"error\":\"1\"}";
    
    webSocket.sendTXT(responseMsg);
    
    digitalWrite(LED_ERROR, HIGH);
    tTurnOffLeds.setInterval(200);
    tTurnOffLeds.restartDelayed();
    
    Serial.println("❌ Timeout");
}

// ========== Turn Off LEDs ==========
void TurnOffLeds() {
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_SUCCESS, LOW);
}

// ========== Start Stimulus ==========
void StimulusStart() {
    digitalWrite(LED_STIMULUS, HIGH);
    stimulusStartTime = millis();
    isStimulating = true;
    
    tStimulusTimeout.setInterval(timeoutDuration);
    tStimulusTimeout.restartDelayed();
    
    Serial.printf("💡 Stimulus ON (timeout: %dms)\n", timeoutDuration);
}

// ========== Setup ==========
void setup() {
    delay(10);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_STIMULUS, OUTPUT);
    pinMode(LED_STATUS, OUTPUT);
    pinMode(LED_ERROR, OUTPUT);
    pinMode(LED_SUCCESS, OUTPUT);
    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    digitalWrite(LED_STIMULUS, LOW);
    digitalWrite(LED_STATUS, HIGH);
    digitalWrite(LED_ERROR, LOW);
    digitalWrite(LED_SUCCESS, LOW);
    
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    
    Serial.println("\n\n\n*****************************");
    Serial.println("*                           *");
    Serial.println("*      TrainerLights        *");
    Serial.println("*      Client v2.0          *");
    Serial.println("*    By: khader afeez     *");
    Serial.println("*  khaderafeez16@gmail.com  *");
    Serial.println("*****************************\n");

    deviceId = generateDeviceId();
    Serial.printf("🔑 Device ID: %s\n", deviceId.c_str());
    
    loadConfig();
    
    wifi_set_sleep_type(NONE_SLEEP_T);
    
    digitalWrite(LED_STATUS, LOW);
    
    Serial.println("⏳ Ready to connect...\n");
}

// ========== Main Loop ==========
void loop() {
    monitorWiFi();
    
    if (webSocketConnected) {
        webSocket.loop();
    }
    
    taskScheduler.execute();
}