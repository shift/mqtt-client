# MQTT 5.0 Client Library

A comprehensive MQTT 5.0 client library for ESP32 platforms built with PlatformIO. This library provides full support for MQTT 5.0 protocol features while maintaining backward compatibility with MQTT 3.1.1.

## Features

### MQTT 5.0 Support

- **Enhanced Authentication**: Support for SCRAM, OAuth, and custom authentication methods
- **Properties System**: Full support for MQTT 5.0 properties on all packet types
- **Reason Codes**: Detailed error reporting with standard MQTT 5.0 reason codes
- **User Properties**: Custom key-value pairs for application-specific metadata
- **Message Expiry**: Automatic message expiration handling
- **Topic Aliases**: Bandwidth optimization through topic aliasing
- **Server Capabilities**: Discovery of broker features and limitations
- **Enhanced Session Management**: Configurable session expiry intervals
- **Flow Control**: Receive maximum and packet size limitations

### Core Features

- Clean, modern C++ API with lambda support
- Backward compatibility with existing MQTT 3.1.1 code
- Non-blocking asynchronous operations
- Comprehensive error handling and logging
- Built-in WiFi integration for ESP platforms
- SSL/TLS support for secure connections
- mTLS (mutual TLS) support with client certificates
- WebSocket transport support
- Extensive unit test coverage

## Quick Start

### Basic Usage (Legacy MQTT 3.1.1 Compatible)

```cpp
#include "MqttClient.h"

MqttClient mqtt;

void setup() {
    // Simple connection (MQTT 3.1.1 compatible)
    mqtt.begin("mqtt://broker.example.com:1883", "username", "password");

    // Set callbacks
    mqtt.onConnect([]() {
        Serial.println("Connected to MQTT broker");
        mqtt.subscribe("sensor/temperature");
    });

    mqtt.onMessage([](const std::string& topic, const std::string& payload) {
        Serial.printf("Received: %s -> %s\n", topic.c_str(), payload.c_str());
    });

    mqtt.connect();
}

void loop() {
    // Publish data
    mqtt.publish("sensor/humidity", "65.2", 1, false);
    delay(5000);
}
```

### WebSocket Transport with Path

You can connect over WebSocket by using a `ws://` or `wss://` URI, including a path if required by your broker:

```cpp
MqttClient* mqtt = MqttClient::getInstance();

// Example: WebSocket with custom path
mqtt->begin("wss://broker.example.com:443/mqtt");
mqtt->setCredentials("username", "password");
mqtt->setProtocolFallback(true); // optional
mqtt->connect("my-client-id");
```

Alternatively, you can configure WebSocket transport and path programmatically:

```cpp
mqtt->setServer("broker.example.com", 443);
mqtt->setWebSocket(true);
mqtt->setPath("/mqtt");
mqtt->connect("my-client-id");
```

### Secure Connection with TLS

Connect to a broker using TLS encryption:

```cpp
MqttClient* mqtt = MqttClient::getInstance();

// Connect using mqtts:// scheme
mqtt->begin("mqtts://broker.example.com:8883");
mqtt->setCredentials("username", "password");

// Optional: Set CA certificate for server verification
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
...
-----END CERTIFICATE-----
)EOF";

mqtt->setCACert(ca_cert);
mqtt->connect("my-client-id");
```

### mTLS (Mutual TLS) with Client Certificates

For mutual TLS authentication where both the client and server verify each other:

```cpp
MqttClient* mqtt = MqttClient::getInstance();

// CA certificate to verify the server
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
...
-----END CERTIFICATE-----
)EOF";

// Client certificate for authentication
const char* client_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDWjCCAkKgAwIBAgIVANVGz4XV9VlBCPBcVCLgFqHFPqLCMA0GCSqGSIb3DQEB
...
-----END CERTIFICATE-----
)EOF";

// Client private key (keep this secret!)
const char* client_key = R"EOF(
-----BEGIN PRIVATE KEY-----
MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQC9yWlqBe5J8dYX
...
-----END PRIVATE KEY-----
)EOF";

mqtt->begin("mqtts://broker.example.com:8883");
mqtt->setCredentials("username", "password");

// Configure mTLS certificates
mqtt->setCACert(ca_cert);           // Server verification
mqtt->setClientCert(client_cert);   // Client authentication
mqtt->setClientKey(client_key);     // Client private key

mqtt->connect("my-client-id");
```

**Note:** For testing purposes only, you can skip certificate verification:

```cpp
mqtt->setInsecure(true);  // WARNING: Only use for testing!
```

### Advanced Usage (MQTT 5.0 Features)

```cpp
#include "MqttClient.h"

MqttClient mqtt;

void setup() {
    // Configure MQTT 5.0 connection options
    MqttConnectOptions options;
    options.clientId = "my-iot-device";
    options.username = "device_user";
    options.password = "secure_password";
    options.keepalive = 60;
    options.cleanStart = true;
    options.sessionExpiryInterval = 3600; // 1 hour
    options.receiveMaximum = 100;
    options.requestResponseInformation = true;

    // Add user properties
    options.userProperties["device_type"] = "sensor";
    options.userProperties["firmware_version"] = "1.2.3";

    // Configure will message with MQTT 5.0 properties
    options.willFlag = true;
    options.willTopic = "device/status";
    options.willPayload = "{\"status\":\"offline\"}";
    options.willQos = 1;
    options.willRetain = true;
    options.willDelayInterval = 30;

    mqtt.begin("mqtt://broker.example.com:1883", options);

    // Enhanced callbacks with MQTT 5.0 support
    mqtt.onConnect([](MqttReasonCode code, const std::vector<MqttProperty>& props) {
        Serial.printf("Connected with reason: %d\n", static_cast<int>(code));
        Serial.printf("Server capabilities received: %zu properties\n", props.size());

        // Subscribe with MQTT 5.0 options
        MqttSubscribeOptions subOpts;
        subOpts.qos = 1;
        subOpts.subscriptionIdentifier = 42;
        subOpts.userProperties["priority"] = "high";

        mqtt.subscribe("sensor/+", subOpts);
    });

    mqtt.onMessage([](const std::string& topic, const std::string& payload,
                     const std::vector<MqttProperty>& properties) {
        Serial.printf("Topic: %s, Payload: %s\n", topic.c_str(), payload.c_str());

        // Process MQTT 5.0 properties
        for (const auto& prop : properties) {
            switch (prop.type) {
                case MqttPropertyType::CONTENT_TYPE:
                    Serial.printf("Content-Type: %s\n", prop.stringValue.c_str());
                    break;
                case MqttPropertyType::RESPONSE_TOPIC:
                    Serial.printf("Response-Topic: %s\n", prop.stringValue.c_str());
                    break;
                case MqttPropertyType::USER_PROPERTY:
                    Serial.println("User property found");
                    break;
            }
        }
    });

    mqtt.connect();
}

void loop() {
    // Publish with MQTT 5.0 properties
    MqttPublishOptions pubOpts;
    pubOpts.qos = 1;
    pubOpts.retain = false;
    pubOpts.messageExpiryInterval = 300; // 5 minutes
    pubOpts.contentType = "application/json";
    pubOpts.responseTopic = "sensor/response";
    pubOpts.userProperties["timestamp"] = String(millis()).c_str();

    String payload = "{\"temperature\": 23.5, \"humidity\": 65.2}";
    mqtt.publish("sensor/data", payload.c_str(), pubOpts);

    delay(10000);
}
```

## API Reference

### Connection Management

#### `void begin(const std::string& brokerUri, const MqttConnectOptions& options)`

Initialize the MQTT client with full MQTT 5.0 options.

#### `void begin(const std::string& brokerUri, const std::string& username, const std::string& password, uint16_t keepalive)`

Legacy initialization method for backward compatibility.

#### `bool connect()`

Establish connection to the MQTT broker.

#### `void disconnect(MqttReasonCode reasonCode, const std::vector<MqttProperty>& properties)`

Disconnect with MQTT 5.0 reason code and properties.

### Security & TLS/mTLS Configuration

#### `void setCACert(const char* ca_cert)`

Set CA certificate for server verification. The certificate should be in PEM format.

#### `void setClientCert(const char* client_cert)`

Set client certificate for mTLS authentication. The certificate should be in PEM format.

#### `void setClientKey(const char* client_key)`

Set client private key for mTLS authentication. The key should be in PEM format.

#### `void setInsecure(bool insecure)`

Skip certificate verification (for testing only). **WARNING:** Only use this for development/testing. Never use in production.

### Publishing

#### `int publish(const std::string& topic, const std::string& payload, const MqttPublishOptions& options)`

Publish message with MQTT 5.0 options and properties.

#### `int publish(const std::string& topic, const std::string& payload, int qos, bool retain)`

Legacy publish method for backward compatibility.

### Subscribing

#### `int subscribe(const std::string& topic, const MqttSubscribeOptions& options)`

Subscribe to topic with MQTT 5.0 options.

#### `int subscribe(const std::vector<std::pair<std::string, MqttSubscribeOptions>>& subscriptions)`

Batch subscribe to multiple topics with individual options.

#### `int subscribe(const std::string& topic, int qos)`

Legacy subscribe method for backward compatibility.

### Callbacks

#### `void onMessage(MessageCallback cb)`

Set message received callback with MQTT 5.0 properties support.

#### `void onConnect(ConnectCallback cb)`

Set connection established callback with reason code and properties.

#### `void onDisconnect(DisconnectCallback cb)`

Set disconnection callback with reason code and properties.

#### `void onPublish(PublishCallback cb)`

Set publish acknowledgment callback with reason code.

#### `void onSubscribe(SubscribeCallback cb)`

Set subscription acknowledgment callback with reason codes.

### Server Capabilities (MQTT 5.0)

#### `uint16_t getServerKeepAlive()`

Get the keep-alive value assigned by the server.

#### `std::string getAssignedClientIdentifier()`

Get the client identifier assigned by the server.

#### `uint16_t getTopicAliasMaximum()`

Get the maximum topic alias value supported by the server.

#### `uint32_t getMaximumPacketSize()`

Get the maximum packet size allowed by the server.

#### `bool isRetainAvailable()`

Check if the server supports retained messages.

#### `bool isWildcardSubscriptionAvailable()`

Check if the server supports wildcard subscriptions.

#### `bool isSharedSubscriptionAvailable()`

Check if the server supports shared subscriptions.

## MQTT 5.0 Structures

### MqttConnectOptions

Comprehensive connection configuration with all MQTT 5.0 options:

- Client identification and authentication
- Session management (clean start, session expiry)
- Flow control (receive maximum, packet size limits)
- Feature requests (response info, problem info)
- Enhanced authentication support
- Will message with properties
- User properties for custom metadata

### MqttPublishOptions

Publishing options with MQTT 5.0 enhancements:

- Message expiry interval
- Topic aliasing for bandwidth optimization
- Content type specification
- Response topic for request/response patterns
- Correlation data for message correlation
- User properties for application metadata

### MqttSubscribeOptions

Subscription options with MQTT 5.0 features:

- Subscription identifiers for message routing
- User properties for subscription metadata
- Enhanced QoS handling

### MqttProperty

Type-safe property handling for all MQTT 5.0 property types:

- String properties (content type, response topic, etc.)
- Integer properties (expiry intervals, maximums, etc.)
- Binary properties (correlation data, authentication data)

### MqttReasonCode

Comprehensive reason codes for detailed error reporting:

- Success and informational codes
- Client and server error codes
- Protocol violation indicators
- Feature availability indicators

## Platform Support

- **ESP32**: Full feature support including mTLS with Arduino Framework and ESP-IDF
- **ESP8266**: ⚠️ **Not currently supported** - This library uses ESP-IDF's MQTT client which is ESP32-only
- **Native**: Unit testing and development support

## Build Configuration

The library automatically configures MQTT 5.0 support through build flags:

- `MQTT_PROTOCOL_5`: Enable MQTT 5.0 protocol support
- `CONFIG_MQTT_PROTOCOL_5`: ESP-IDF specific MQTT 5.0 configuration
- `CONFIG_MQTT_TRANSPORT_SSL`: Enable SSL/TLS transport
- `CONFIG_MQTT_TRANSPORT_WEBSOCKET`: Enable WebSocket transport

## Testing

Run unit tests for both embedded and native platforms:

```bash
# Test on native platform
pio test -e native

# Test on ESP32
pio test -e esp32
```

**Note:** ESP8266 is not currently supported as this library uses ESP-IDF's MQTT client APIs.

## Examples

See the `examples/` directory for comprehensive usage examples:

- `basic_usage/`: Complete MQTT 5.0 example with all features demonstrated
- Advanced authentication patterns
- Property usage examples
- Error handling demonstrations

## License

MIT License - see LICENSE file for details.

## Contributing

Contributions are welcome! Please ensure:

1. All tests pass on supported platforms
2. New features include comprehensive unit tests
3. Documentation is updated for API changes
4. Code follows the existing style conventions

## Changelog

### v1.0.1

- Full MQTT 5.0 protocol support
- Enhanced property system
- Comprehensive reason code support
- Backward compatibility maintained
- Extensive unit test coverage
- Enhanced authentication support
- Server capability discovery
- Flow control implementation
