#include <Arduino.h>
#include <WiFi.h>
#include "MqttClient.h"

// WiFi credentials
const char *ssid = "your_wifi_ssid";
const char *password = "your_wifi_password";

// MQTT broker settings - using mqtts:// for TLS
const char *mqtt_broker = "mqtts://broker.example.com:8883";
const char *mqtt_username = "device_user";
const char *mqtt_password = "device_password";
const char *client_id = "esp32_mtls_client";

// CA Certificate for server verification
// Replace with your broker's CA certificate
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
...
-----END CERTIFICATE-----
)EOF";

// Client Certificate for mTLS authentication
// Replace with your device's client certificate
const char* client_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVANVGz4XV9VlBCPBcVCLgFqHFPqLCMA0GCSqGSIb3DQEB
CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t
IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yMzAxMTUxMjM0
...
-----END CERTIFICATE-----
)EOF";

// Client Private Key for mTLS authentication
// IMPORTANT: Keep this secret! Never commit to version control
const char* client_key = R"EOF(
-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQC9yWlqBe5J8dYX
VKnhZJqLW3h0F5g8n2sLqU0p7Y+8k9VnB8xMNOXhYDlKBzLvK8Q5pXlZ3y7h8Wz
...
-----END PRIVATE KEY-----
)EOF";

MqttClient* mqtt;

void onMqttMessage(const char *topic, const char *payload, size_t length)
{
  Serial.printf("[MQTT] Message received on %s: ", topic);
  Serial.write((const uint8_t*)payload, length);
  Serial.println();
  
  // Example: Handle specific topics
  if (strcmp(topic, "device/command") == 0) {
    Serial.println("Received command from server");
    // Process command...
  }
}

void onConnected()
{
  Serial.println("✓ Successfully connected to MQTT broker with mTLS!");
  Serial.println("  Server verified client certificate");
  
  // Subscribe to topics after connection
  mqtt->subscribe("device/command", 1);
  mqtt->subscribe("device/config", 1);
  mqtt->subscribe("sensor/#", 0);
  
  // Publish connection status
  mqtt->publish("device/status", "{\"status\":\"online\",\"auth\":\"mtls\"}", true);
}

void onDisconnected()
{
  Serial.println("✗ Disconnected from MQTT broker");
}

void connectToWiFi()
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("✓ Connected to WiFi\n");
  Serial.printf("  IP address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("  Signal strength: %d dBm\n", WiFi.RSSI());
}

void setup()
{
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  MQTT Client with mTLS Example        ║");
  Serial.println("║  Mutual TLS Authentication            ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  // Connect to WiFi
  connectToWiFi();
  
  // Get MQTT client instance
  mqtt = MqttClient::getInstance();
  
  // Set up event callbacks
  mqtt->onMessage(onMqttMessage);
  mqtt->onConnect(onConnected);
  mqtt->onDisconnect(onDisconnected);
  
  // Configure broker connection
  mqtt->begin(mqtt_broker);
  mqtt->setCredentials(mqtt_username, mqtt_password);
  
  // Configure mTLS certificates
  Serial.println("\n[Setup] Configuring mTLS certificates...");
  mqtt->setCACert(ca_cert);           // Verify server's certificate
  mqtt->setClientCert(client_cert);   // Present client certificate
  mqtt->setClientKey(client_key);     // Client private key
  
  // Optional: Enable protocol fallback to MQTT 3.1.1 if v5 fails
  mqtt->setProtocolFallback(true);
  
  // Optional: For testing only - skip certificate verification
  // WARNING: Never use in production!
  // mqtt->setInsecure(true);
  
  Serial.println("[Setup] Connecting to broker with mTLS...");
  Serial.printf("  Broker: %s\n", mqtt_broker);
  Serial.printf("  Client ID: %s\n", client_id);
  Serial.println("  Auth: Client Certificate + Username/Password");
  
  // Connect to the broker
  if (mqtt->connect(client_id)) {
    Serial.println("✓ Connection initiated");
  } else {
    Serial.println("✗ Failed to initiate connection");
  }
}

void loop()
{
  static unsigned long lastPublish = 0;
  static int counter = 0;
  
  // Publish sensor data every 10 seconds
  if (mqtt->isConnected() && millis() - lastPublish > 10000)
  {
    lastPublish = millis();
    
    // Simulate sensor readings
    float temperature = 20.0 + (random(100) / 10.0);
    float humidity = 50.0 + (random(200) / 10.0);
    
    // Create JSON payload
    char payload[128];
    snprintf(payload, sizeof(payload), 
             "{\"temp\":%.1f,\"humidity\":%.1f,\"count\":%d,\"uptime\":%lu}",
             temperature, humidity, counter++, millis() / 1000);
    
    Serial.printf("[Publish] Sending sensor data: %s\n", payload);
    int msg_id = mqtt->publish("sensor/data", payload, false);
    
    if (msg_id > 0) {
      Serial.printf("  ✓ Published with msg_id: %d\n", msg_id);
    } else {
      Serial.println("  ✗ Publish failed");
    }
  }
  
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost! Reconnecting...");
    connectToWiFi();
  }
  
  delay(100);
}
