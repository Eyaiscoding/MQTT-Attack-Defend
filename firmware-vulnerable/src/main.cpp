/**
 * @file main.cpp
 * @brief ESP32 MQTT IoT Node - Phase 1 (Vulnerable Configuration)
 * 
 * @project SUP'COM MIT Project 4: MQTT Attack & Defense
 * @author Eya
 * @date November 2026
 * @version 0.2.0
 * 
 * @description
 * ESP32 firmware for sensor-actuator node with intentionally vulnerable 
 * MQTT configuration (no TLS, no authentication). Reads DHT11 temperature
 * and humidity data, publishes to MQTT broker, and controls a relay/LED
 * based on subscribed commands.
 * 
 * @hardware
 * - ESP32 DevKit (ESP32-WROOM-32)
 * - DHT11 Temperature & Humidity Sensor (GPIO 4)
 * - LED or Relay Module (GPIO 2)
 * 
 * @features
 * - Non-blocking Wi-Fi connection with auto-reconnect
 * - MQTT client with QoS 0/1 support
 * - JSON payload serialization
 * - Relay control via MQTT subscription callback
 * - Performance diagnostics (connection time, heap memory)
 * - Serial debug output for monitoring
 * 
 * @security_notes
 * ⚠️ INTENTIONALLY VULNERABLE - Phase 1 Only:
 * - No TLS encryption (port 1883)
 * - No authentication or authorization
 * - No input validation on actuator commands
 * - Cleartext MQTT traffic (Wireshark-visible)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// ============================================================================
// CONFIGURATION - MODIFY THESE VALUES FOR YOUR SETUP
// ============================================================================

// Wi-Fi Credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";           // Replace with your Wi-Fi SSID
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   // Replace with your Wi-Fi password

// MQTT Broker Configuration (Raspberry Pi)
const char* MQTT_BROKER = "192.168.1.100";          // Replace with Raspberry Pi IP
const int MQTT_PORT = 1883;                         // Non-encrypted MQTT port
const char* MQTT_CLIENT_ID = "ESP32_Node_01";      // Unique client identifier

// MQTT Topics
const char* TOPIC_SENSOR = "iot/projet4/capteurs/temperature";
const char* TOPIC_STATUS = "iot/projet4/status";
const char* TOPIC_RELAY_CMD = "iot/projet4/actionneurs/relais";
const char* TOPIC_RELAY_STATE = "iot/projet4/actionneurs/relais/state";

// GPIO Pin Definitions
#define DHT_PIN 4           // DHT11 Data pin (with 10kΩ pull-up to 3.3V)
#define DHT_TYPE DHT11      // DHT sensor type
#define RELAY_PIN 2         // Relay/LED control pin (active HIGH)

// Timing Configuration
#define SENSOR_INTERVAL 5000     // Sensor reading interval (ms) - 5 seconds
#define RECONNECT_DELAY 5000     // Wi-Fi/MQTT reconnection delay (ms)
#define WIFI_TIMEOUT 10000       // Wi-Fi connection timeout (ms)

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

WiFiClient espClient;                    // Wi-Fi client
PubSubClient mqttClient(espClient);      // MQTT client
DHT dht(DHT_PIN, DHT_TYPE);              // DHT sensor object

// ============================================================================
// STATE VARIABLES
// ============================================================================

unsigned long lastSensorRead = 0;        // Last sensor reading timestamp
unsigned long bootTime = 0;              // Boot timestamp for uptime calculation
bool relayState = false;                 // Current relay state (false = OFF, true = ON)

// Performance metrics
unsigned long wifiConnectTime = 0;
unsigned long mqttConnectTime = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void setupWiFi();
void reconnectWiFi();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData();
void publishStatus();
void controlRelay(bool state);
void publishRelayState();

// ============================================================================
// SETUP - Runs once at boot
// ============================================================================

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);  // Allow serial monitor to initialize
  
  Serial.println("\n\n========================================");
  Serial.println("ESP32 MQTT IoT Node - Phase 1 (Vulnerable)");
  Serial.println("SUP'COM Project 4: Attack & Defense");
  Serial.println("========================================\n");
  
  bootTime = millis();
  
  // Initialize GPIO pins
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Start with relay OFF
  Serial.println("[GPIO] Relay pin initialized (GPIO 2)");
  
  // Initialize DHT sensor
  dht.begin();
  Serial.println("[DHT11] Sensor initialized (GPIO 4)");
  
  // Connect to Wi-Fi
  setupWiFi();
  
  // Configure MQTT client
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  Serial.printf("[MQTT] Broker configured: %s:%d\n", MQTT_BROKER, MQTT_PORT);
  
  // Connect to MQTT broker
  reconnectMQTT();
  
  // Publish initial status
  publishStatus();
  
  Serial.println("\n[SYSTEM] Initialization complete. Entering main loop...\n");
}

// ============================================================================
// MAIN LOOP - Runs continuously
// ============================================================================

void loop() {
  // Ensure Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection lost. Reconnecting...");
    reconnectWiFi();
  }
  
  // Ensure MQTT connection
  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Connection lost. Reconnecting...");
    reconnectMQTT();
  }
  
  // Process MQTT messages (non-blocking)
  mqttClient.loop();
  
  // Publish sensor data at regular intervals
  unsigned long currentTime = millis();
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentTime;
    publishSensorData();
  }
  
  // Small delay to prevent watchdog timer issues
  delay(10);
}

// ============================================================================
// WI-FI FUNCTIONS
// ============================================================================

/**
 * @brief Initial Wi-Fi connection with performance measurement
 */
void setupWiFi() {
  Serial.printf("[WIFI] Connecting to SSID: %s\n", WIFI_SSID);
  
  WiFi.mode(WIFI_STA);  // Station mode (client)
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  unsigned long timeout = startTime + WIFI_TIMEOUT;
  
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnectTime = millis() - startTime;
    Serial.printf("\n[WIFI] Connected in %lu ms\n", wifiConnectTime);
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("\n[WIFI] Connection timeout! Check credentials and try again.");
    Serial.println("[WIFI] Will retry in main loop...");
  }
}

/**
 * @brief Non-blocking Wi-Fi reconnection
 */
void reconnectWiFi() {
  WiFi.disconnect();
  delay(1000);
  setupWiFi();
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================

/**
 * @brief Connect to MQTT broker with performance measurement
 */
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] Attempting connection to %s:%d...\n", MQTT_BROKER, MQTT_PORT);
    
    unsigned long startTime = millis();
    
    // Attempt connection (no username/password - vulnerable!)
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      mqttConnectTime = millis() - startTime;
      Serial.printf("[MQTT] Connected in %lu ms\n", mqttConnectTime);
      Serial.printf("[MQTT] Client ID: %s\n", MQTT_CLIENT_ID);
      
      // Subscribe to relay command topic
      mqttClient.subscribe(TOPIC_RELAY_CMD, 1);  // QoS 1
      Serial.printf("[MQTT] Subscribed to: %s (QoS 1)\n", TOPIC_RELAY_CMD);
      
      // Report diagnostics
      Serial.printf("[HEAP] Free heap memory: %d bytes\n", ESP.getFreeHeap());
      
    } else {
      Serial.printf("[MQTT] Connection failed, rc=%d\n", mqttClient.state());
      Serial.println("[MQTT] Error codes: -4=timeout, -3=conn lost, -2=fail, -1=disconnected");
      Serial.printf("[MQTT] Retrying in %d seconds...\n", RECONNECT_DELAY / 1000);
      delay(RECONNECT_DELAY);
    }
  }
}

/**
 * @brief MQTT message callback - handles incoming subscribed messages
 * 
 * @param topic Topic name
 * @param payload Message payload (byte array)
 * @param length Payload length
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("[MQTT] Message received on topic: %s\n", topic);
  Serial.printf("[MQTT] Payload: %s\n", message.c_str());
  
  // Handle relay control commands
  if (strcmp(topic, TOPIC_RELAY_CMD) == 0) {
    message.trim();  // Remove whitespace
    message.toUpperCase();  // Case-insensitive comparison
    
    if (message == "ON" || message == "1") {
      Serial.println("[RELAY] Command: Turn ON");
      controlRelay(true);
    } 
    else if (message == "OFF" || message == "0") {
      Serial.println("[RELAY] Command: Turn OFF");
      controlRelay(false);
    } 
    else {
      Serial.printf("[RELAY] ⚠️ Unknown command: %s (expected: ON/OFF/1/0)\n", message.c_str());
      // Note: In Phase 1, we accept invalid commands without error (vulnerability)
    }
  }
}

// ============================================================================
// SENSOR FUNCTIONS
// ============================================================================

/**
 * @brief Read DHT11 sensor and publish data to MQTT broker
 */
void publishSensorData() {
  Serial.println("\n--- Sensor Reading Cycle ---");
  
  // Read temperature and humidity
  float temperature = dht.readTemperature();  // Celsius
  float humidity = dht.readHumidity();        // Percentage
  
  // Check if reading failed
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT11] ⚠️ Failed to read from sensor! Check wiring and pull-up resistor.");
    return;
  }
  
  Serial.printf("[DHT11] Temperature: %.1f°C\n", temperature);
  Serial.printf("[DHT11] Humidity: %.1f%%\n", humidity);
  
  // Calculate uptime in seconds
  unsigned long uptimeSec = (millis() - bootTime) / 1000;
  
  // Create JSON payload
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["temp"] = round(temperature * 10) / 10.0;  // Round to 1 decimal
  jsonDoc["hum"] = round(humidity * 10) / 10.0;
  jsonDoc["uptime"] = uptimeSec;
  
  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);
  
  // Publish to MQTT
  bool success = mqttClient.publish(TOPIC_SENSOR, jsonBuffer, false);  // QoS 0, not retained
  
  if (success) {
    Serial.printf("[PUBLISH] Topic: %s\n", TOPIC_SENSOR);
    Serial.printf("[PUBLISH] Payload: %s\n", jsonBuffer);
    Serial.println("[PUBLISH] ✓ Success");
  } else {
    Serial.println("[PUBLISH] ✗ Failed (check MQTT connection)");
  }
  
  Serial.printf("[HEAP] Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println("--- End Sensor Cycle ---\n");
}

/**
 * @brief Publish node status to MQTT broker (retained message)
 */
void publishStatus() {
  Serial.println("[STATUS] Publishing node status...");
  
  // Create JSON payload
  StaticJsonDocument<300> jsonDoc;
  jsonDoc["state"] = "online";
  jsonDoc["ip"] = WiFi.localIP().toString();
  jsonDoc["rssi"] = WiFi.RSSI();
  jsonDoc["heap"] = ESP.getFreeHeap();
  jsonDoc["uptime"] = (millis() - bootTime) / 1000;
  jsonDoc["client_id"] = MQTT_CLIENT_ID;
  
  char jsonBuffer[300];
  serializeJson(jsonDoc, jsonBuffer);
  
  // Publish with QoS 1 and retained flag
  bool success = mqttClient.publish(TOPIC_STATUS, jsonBuffer, true);  // Retained = true
  
  if (success) {
    Serial.printf("[STATUS] Published to: %s\n", TOPIC_STATUS);
    Serial.printf("[STATUS] Payload: %s\n", jsonBuffer);
  } else {
    Serial.println("[STATUS] ✗ Failed to publish");
  }
}

// ============================================================================
// ACTUATOR FUNCTIONS
// ============================================================================

/**
 * @brief Control relay/LED and publish confirmation
 * 
 * @param state Desired relay state (true = ON, false = OFF)
 */
void controlRelay(bool state) {
  relayState = state;
  digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  
  Serial.printf("[RELAY] Physical state changed: %s\n", state ? "ON (HIGH)" : "OFF (LOW)");
  
  // Publish confirmation
  publishRelayState();
}

/**
 * @brief Publish current relay state to MQTT broker
 */
void publishRelayState() {
  // Create JSON payload
  StaticJsonDocument<150> jsonDoc;
  jsonDoc["relay"] = relayState ? "ON" : "OFF";
  jsonDoc["timestamp"] = millis() / 1000;  // Unix-like timestamp (seconds since boot)
  
  char jsonBuffer[150];
  serializeJson(jsonDoc, jsonBuffer);
  
  // Publish with retained flag so last state is always available
  bool success = mqttClient.publish(TOPIC_RELAY_STATE, jsonBuffer, true);  // QoS 0, retained
  
  if (success) {
    Serial.printf("[RELAY] State published: %s\n", jsonBuffer);
  } else {
    Serial.println("[RELAY] ✗ Failed to publish state");
  }
}

// ============================================================================
// DIAGNOSTICS (Phase 2 Comparison Baseline)
// ============================================================================

/**
 * Performance metrics logged during boot:
 * - Wi-Fi connection time: typically 2000-4000 ms
 * - MQTT connection time: typically 200-500 ms (without TLS)
 * - Free heap memory: typically 230,000-250,000 bytes
 * 
 * These values will be compared with Phase 2 (TLS enabled) to measure
 * the overhead of encryption and certificate validation.
 */
