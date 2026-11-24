/**
 * @file main.cpp
 * @brief ESP32 MQTT IoT Node - Phase 2 (Hardened with TLS/mTLS)
 * 
 * @project SUP'COM MIT Project 4: MQTT Attack & Defense
 * @author Eya
 * @date November 2026
 * @version 1.0.0
 * 
 * @description
 * ESP32 firmware for sensor-actuator node with HARDENED security configuration:
 * - MQTT over TLS (MQTTS) on port 8883
 * - Mutual TLS authentication (mTLS) with X.509 client certificates
 * - Hardware-accelerated crypto (AES, SHA, RSA via ESP32 crypto engine)
 * 
 * This firmware maintains IDENTICAL functionality to the vulnerable version
 * (DHT11 sensor, relay control, MQTT topics) to enable direct performance
 * comparison and demonstrate the security improvement.
 * 
 * @hardware
 * - ESP32 DevKit (ESP32-WROOM-32)
 * - DHT11 Temperature & Humidity Sensor (GPIO 4)
 * - LED or Relay Module (GPIO 2)
 * 
 * @features
 * - Non-blocking Wi-Fi connection with auto-reconnect
 * - MQTTS client with QoS 0/1 support (port 8883)
 * - Mutual TLS (mTLS) with client certificate authentication
 * - CA certificate validation of broker
 * - JSON payload serialization
 * - Relay control via MQTT subscription callback
 * - Performance diagnostics (connection time, TLS handshake, heap memory)
 * - Serial debug output for monitoring
 * 
 * @security_features
 * ✅ TLS 1.2 encryption (port 8883)
 * ✅ X.509 client certificate authentication
 * ✅ CA certificate verification
 * ✅ ESP32 hardware crypto acceleration
 * ✅ Encrypted MQTT traffic (not visible in Wireshark)
 * ✅ Protection against MitM, eavesdropping, replay attacks
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>  // TLS/SSL support (replaces WiFiClient)
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// Include embedded certificates
// NOTE: Copy certs.h.example to certs.h and embed actual certificates
#include "certs.h"

// ============================================================================
// CONFIGURATION - MODIFY THESE VALUES FOR YOUR SETUP
// ============================================================================

// Wi-Fi Credentials
const char* WIFI_SSID = "YOUR_WIFI_SSID";           // Replace with your Wi-Fi SSID
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   // Replace with your Wi-Fi password

// MQTT Broker Configuration (Raspberry Pi with TLS)
const char* MQTT_BROKER = "192.168.1.100";          // Replace with Raspberry Pi IP
const int MQTT_PORT = 8883;                         // TLS-encrypted MQTT port (8883)
const char* MQTT_CLIENT_ID = "esp32-node-01";      // Must match CN in client certificate!

// MQTT Topics (IDENTICAL to vulnerable firmware for comparison)
const char* TOPIC_SENSOR = "iot/projet4/capteurs/temperature";
const char* TOPIC_STATUS = "iot/projet4/status";
const char* TOPIC_RELAY_CMD = "iot/projet4/actionneurs/relais";
const char* TOPIC_RELAY_STATE = "iot/projet4/actionneurs/relais/state";

// GPIO Pin Definitions (IDENTICAL to vulnerable firmware)
#define DHT_PIN 4           // DHT11 Data pin (with 10kΩ pull-up to 3.3V)
#define DHT_TYPE DHT11      // DHT sensor type
#define RELAY_PIN 2         // Relay/LED control pin (active HIGH)

// Timing Configuration
#define SENSOR_INTERVAL 5000     // Sensor reading interval (ms) - 5 seconds
#define RECONNECT_DELAY 5000     // Wi-Fi/MQTT reconnection delay (ms)
#define WIFI_TIMEOUT 10000       // Wi-Fi connection timeout (ms)

// Benchmark Mode Configuration
// Set to 1 to enable automated 50-message benchmark cycle with CSV output
// Set to 0 for normal operation
#define BENCHMARK_MODE 0
#define BENCHMARK_MESSAGES 50    // Number of messages to send in benchmark mode

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

WiFiClientSecure espClient;              // TLS/SSL-enabled Wi-Fi client
PubSubClient mqttClient(espClient);      // MQTT client using secure connection
DHT dht(DHT_PIN, DHT_TYPE);              // DHT sensor object

// ============================================================================
// STATE VARIABLES
// ============================================================================

unsigned long lastSensorRead = 0;        // Last sensor reading timestamp
unsigned long bootTime = 0;              // Boot timestamp for uptime calculation
bool relayState = false;                 // Current relay state (false = OFF, true = ON)

// Performance metrics (for TLS overhead measurement)
unsigned long wifiConnectTime = 0;
unsigned long mqttConnectTime = 0;       // Includes TLS handshake time
unsigned long tlsHandshakeTime = 0;      // TLS handshake duration

// Benchmark mode state
#if BENCHMARK_MODE
int benchmarkMessageCount = 0;
bool benchmarkComplete = false;
#endif

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

void setupWiFi();
void reconnectWiFi();
void setupTLS();
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
  Serial.println("ESP32 MQTT IoT Node - Phase 2 (Hardened)");
  Serial.println("SUP'COM Project 4: Attack & Defense");
  Serial.println("TLS/mTLS Enabled - Port 8883");
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
  
  // Setup TLS/SSL with certificates
  setupTLS();
  
  // Configure MQTT client
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  
  Serial.printf("[MQTT] Broker configured: %s:%d (TLS enabled)\n", MQTT_BROKER, MQTT_PORT);
  
  // Connect to MQTT broker over TLS
  reconnectMQTT();
  
  // Publish initial status
  publishStatus();
  
  Serial.println("\n[SYSTEM] Initialization complete. Entering main loop...\n");
  
#if BENCHMARK_MODE
  Serial.println("========================================");
  Serial.println("BENCHMARK MODE ENABLED (TLS)");
  Serial.println("========================================");
  Serial.println("Will automatically publish 50 messages and output CSV data");
  Serial.println("CSV Format: version,msg_id,connect_ms,publish_ms,free_heap_bytes,min_free_heap_bytes");
  Serial.println("Copy CSV lines to benchmarks/sample_data/tls.csv for analysis");
  Serial.println("========================================\n");
#endif
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
  
#if BENCHMARK_MODE
  // In benchmark mode, publish messages as fast as possible
  if (!benchmarkComplete && benchmarkMessageCount < BENCHMARK_MESSAGES) {
    publishSensorData();
    benchmarkMessageCount++;
    
    if (benchmarkMessageCount >= BENCHMARK_MESSAGES) {
      benchmarkComplete = true;
      Serial.println("\n========================================");
      Serial.println("BENCHMARK COMPLETE (TLS)");
      Serial.println("========================================");
      Serial.printf("Total messages sent: %d\n", BENCHMARK_MESSAGES);
      Serial.printf("WiFi connect time: %lu ms\n", wifiConnectTime);
      Serial.printf("MQTT+TLS connect time: %lu ms\n", mqttConnectTime);
      Serial.printf("TLS handshake time: %lu ms\n", tlsHandshakeTime);
      Serial.printf("Final free heap: %d bytes\n", ESP.getFreeHeap());
      Serial.printf("Minimum free heap: %d bytes\n", ESP.getMinFreeHeap());
      Serial.println("========================================");
      Serial.println("Copy CSV lines above to tls.csv");
      Serial.println("Reset ESP32 to run benchmark again");
      Serial.println("========================================\n");
    }
    delay(100);  // Small delay between benchmark messages
  }
#else
  // Normal mode: publish at regular intervals
  if (currentTime - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentTime;
    publishSensorData();
  }
#endif
  
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
// TLS/SSL FUNCTIONS
// ============================================================================

/**
 * @brief Configure TLS/SSL client with certificates for mutual authentication
 * 
 * This function sets up WiFiClientSecure with:
 * - CA certificate (to verify broker's server certificate)
 * - Client certificate (to authenticate ESP32 to broker)
 * - Client private key (for mTLS handshake)
 */
void setupTLS() {
  Serial.println("\n[TLS] Configuring TLS/SSL client with certificates...");
  
  // Set CA certificate for server verification
  espClient.setCACert(CA_CERT);
  Serial.println("[TLS] ✓ CA certificate loaded (verify broker identity)");
  
  // Set client certificate and private key for mutual authentication (mTLS)
  espClient.setCertificate(CLIENT_CERT);
  Serial.println("[TLS] ✓ Client certificate loaded (authenticate ESP32)");
  
  espClient.setPrivateKey(CLIENT_KEY);
  Serial.println("[TLS] ✓ Client private key loaded");
  
  // Report certificate sizes (for memory diagnostics)
  Serial.printf("[TLS] Certificate storage in Flash (PROGMEM): %d bytes\n", TOTAL_CERT_SIZE);
  Serial.printf("[TLS]   - CA cert: %d bytes\n", CA_CERT_SIZE);
  Serial.printf("[TLS]   - Client cert: %d bytes\n", CLIENT_CERT_SIZE);
  Serial.printf("[TLS]   - Client key: %d bytes\n", CLIENT_KEY_SIZE);
  
  Serial.println("[TLS] TLS client configured successfully\n");
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================

/**
 * @brief Connect to MQTT broker over TLS with performance measurement
 * 
 * This function performs:
 * 1. TLS handshake with broker (includes certificate exchange and validation)
 * 2. MQTT CONNECT packet exchange
 * 
 * Performance note: TLS handshake adds ~500-1500ms compared to plaintext MQTT
 */
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] Attempting TLS connection to %s:%d...\n", MQTT_BROKER, MQTT_PORT);
    Serial.printf("[MQTT] Client ID (CN): %s\n", MQTT_CLIENT_ID);
    
    // Check free heap before TLS connection (TLS requires ~40-60 KB)
    uint32_t freeHeapBefore = ESP.getFreeHeap();
    Serial.printf("[HEAP] Free heap before TLS connection: %u bytes\n", freeHeapBefore);
    
    if (freeHeapBefore < 60000) {
      Serial.println("[WARNING] Low heap memory! TLS connection may fail.");
      Serial.println("[WARNING] Consider reducing buffer sizes or disabling features.");
    }
    
    unsigned long startTime = millis();
    
    // Attempt TLS connection and MQTT handshake
    // Note: MQTT_CLIENT_ID must match the CN in the client certificate!
    bool connected = mqttClient.connect(MQTT_CLIENT_ID);
    
    if (connected) {
      mqttConnectTime = millis() - startTime;
      tlsHandshakeTime = mqttConnectTime;  // Approximation (includes MQTT CONNECT)
      
      Serial.printf("[MQTT] ✓ Connected in %lu ms (includes TLS handshake)\n", mqttConnectTime);
      Serial.printf("[MQTT] Client authenticated as: %s\n", MQTT_CLIENT_ID);
      
      // Report TLS connection details
      Serial.println("[TLS] Handshake successful:");
      Serial.println("[TLS]   - Protocol: TLS 1.2 (or higher)");
      Serial.println("[TLS]   - Cipher: Hardware-accelerated AES");
      Serial.println("[TLS]   - Server certificate: VERIFIED");
      Serial.println("[TLS]   - Client certificate: AUTHENTICATED");
      
      // Subscribe to relay command topic
      mqttClient.subscribe(TOPIC_RELAY_CMD, 1);  // QoS 1
      Serial.printf("[MQTT] Subscribed to: %s (QoS 1)\n", TOPIC_RELAY_CMD);
      
      // Report diagnostics
      uint32_t freeHeapAfter = ESP.getFreeHeap();
      Serial.printf("[HEAP] Free heap after TLS connection: %u bytes\n", freeHeapAfter);
      Serial.printf("[HEAP] TLS overhead: %d bytes\n", freeHeapBefore - freeHeapAfter);
      Serial.printf("[HEAP] Minimum free heap: %u bytes\n", ESP.getMinFreeHeap());
      
    } else {
      int errorCode = mqttClient.state();
      Serial.printf("[MQTT] ✗ Connection failed, rc=%d\n", errorCode);
      Serial.println("[MQTT] Error codes: -4=timeout, -3=conn lost, -2=fail, -1=disconnected");
      
      // Check TLS-specific errors
      int tlsError = espClient.lastError(nullptr, 0);
      if (tlsError != 0) {
        Serial.printf("[TLS] ✗ TLS error code: %d\n", tlsError);
        Serial.println("[TLS] Common TLS errors:");
        Serial.println("[TLS]   - Certificate verification failed (check CA cert)");
        Serial.println("[TLS]   - Client certificate rejected (check CN matches MQTT_CLIENT_ID)");
        Serial.println("[TLS]   - Insufficient heap memory for TLS buffers");
        Serial.println("[TLS]   - Server certificate hostname mismatch (check SANs)");
      }
      
      Serial.printf("[MQTT] Retrying in %d seconds...\n", RECONNECT_DELAY / 1000);
      delay(RECONNECT_DELAY);
    }
  }
}

/**
 * @brief MQTT message callback - handles incoming subscribed messages
 * 
 * @param topic Topic name
 * @param payload Message payload (byte array, decrypted by TLS layer)
 * @param length Payload length
 * 
 * Note: Payload arrives DECRYPTED - TLS decryption is automatic
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.printf("[MQTT] Message received on topic: %s\n", topic);
  Serial.printf("[MQTT] Payload (decrypted): %s\n", message.c_str());
  
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
    }
  }
}

// ============================================================================
// SENSOR FUNCTIONS
// ============================================================================

/**
 * @brief Read DHT11 sensor and publish data to MQTT broker over TLS
 * 
 * Note: JSON payload is encrypted by TLS before transmission
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
  
  // Create JSON payload (IDENTICAL to vulnerable firmware)
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["temp"] = round(temperature * 10) / 10.0;  // Round to 1 decimal
  jsonDoc["hum"] = round(humidity * 10) / 10.0;
  jsonDoc["uptime"] = uptimeSec;
  
  char jsonBuffer[200];
  serializeJson(jsonDoc, jsonBuffer);
  
  // Publish to MQTT (payload encrypted by TLS automatically)
  unsigned long publishStart = millis();
  bool success = mqttClient.publish(TOPIC_SENSOR, jsonBuffer, false);  // QoS 0, not retained
  unsigned long publishTime = millis() - publishStart;
  
  if (success) {
#if BENCHMARK_MODE
    // CSV output for benchmark analysis
    Serial.printf("CSV,tls,%d,%lu,%lu,%d,%d\n",
                  benchmarkMessageCount,
                  mqttConnectTime,
                  publishTime,
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap());
#else
    Serial.printf("[PUBLISH] Topic: %s\n", TOPIC_SENSOR);
    Serial.printf("[PUBLISH] Payload (before encryption): %s\n", jsonBuffer);
    Serial.printf("[PUBLISH] Publish time: %lu ms (includes TLS encryption)\n", publishTime);
    Serial.println("[PUBLISH] ✓ Success (transmitted encrypted over TLS)");
#endif
  } else {
    Serial.println("[PUBLISH] ✗ Failed (check MQTT connection)");
  }
  
#if !BENCHMARK_MODE
  Serial.printf("[HEAP] Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("[HEAP] Min free heap: %d bytes\n", ESP.getMinFreeHeap());
  Serial.println("--- End Sensor Cycle ---\n");
#endif
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
  jsonDoc["tls"] = true;  // Indicate TLS is enabled
  jsonDoc["tls_handshake_ms"] = tlsHandshakeTime;
  
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
    Serial.printf("[RELAY] State published (encrypted): %s\n", jsonBuffer);
  } else {
    Serial.println("[RELAY] ✗ Failed to publish state");
  }
}

// ============================================================================
// DIAGNOSTICS (Phase 2 vs Phase 1 Comparison)
// ============================================================================

/**
 * Performance metrics logged during boot and operation:
 * 
 * PHASE 2 (TLS) EXPECTED VALUES:
 * - Wi-Fi connection time: 2000-4000 ms (unchanged)
 * - MQTT connection time: 700-1500 ms (vs 200-500 ms in Phase 1)
 *   * TLS handshake overhead: +500-1000 ms
 * - Free heap memory: 190,000-210,000 bytes (vs 230,000-250,000 in Phase 1)
 *   * TLS overhead: ~40,000 bytes for buffers and cert chain
 * - Publish latency: 15-45 ms (vs 10-30 ms in Phase 1)
 *   * AES encryption overhead: +5-15 ms per message
 * 
 * SECURITY IMPROVEMENTS:
 * ✅ All MQTT traffic encrypted (AES-128/256-GCM)
 * ✅ Mutual authentication prevents impersonation
 * ✅ CA validation prevents MitM attacks
 * ✅ Wireshark shows only encrypted Application Data (not plaintext JSON)
 * 
 * These metrics are used in benchmarks/analyze_tls_cost.py for
 * quantitative analysis of the security vs. performance tradeoff.
 */
