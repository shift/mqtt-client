#pragma once

#include <functional>

// Forward declarations for ESP-IDF types to avoid hard dependencies in header
#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "mqtt_client.h"
#else
#include "esp_mqtt_client.h"
#endif
#else
#include "esp_mqtt_client.h"
#endif

#include "esp_event.h"
#include "UriUtils.h"

typedef std::function<void(const char* topic, const char* payload, size_t length)> MessageCallback;
typedef std::function<void()> SimpleCallback;

class MqttClient {
public:
  static MqttClient* getInstance();

  void begin(const char* brokerUri);
  void setServer(const char* host, uint16_t port);
  // Enable/disable WebSocket transport. When enabled, a path can be set.
  void setWebSocket(bool enable);
  // Set WebSocket path (e.g., "/mqtt"). Effective when WebSocket is enabled or when using ws/wss URI.
  void setPath(const char* path);
  void setCredentials(const char* username, const char* password);
  void setKeepalive(uint16_t keepalive);
  void setProtocolFallback(bool enableFallback); // Enable v3.1.1 fallback if v5 fails
  
  // mTLS / Certificate configuration
  void setCACert(const char* ca_cert);           // Set CA certificate for server verification
  void setClientCert(const char* client_cert);   // Set client certificate for mTLS
  void setClientKey(const char* client_key);     // Set client private key for mTLS
  void setInsecure(bool insecure);               // Skip certificate verification (for testing only)

  bool connect(const char* clientId);
  void disconnect();
  bool isConnected() const;

  // Communication
  int publish(const char* topic, const char* payload, bool retain = false);
  int subscribe(const char* topic, int qos = 0);
  int unsubscribe(const char* topic);

  // Event callbacks
  void onMessage(MessageCallback cb);
  void onConnect(SimpleCallback cb);
  void onDisconnect(SimpleCallback cb);

  // Processing
  void loop(); // No-op for event-driven esp-mqtt

  ~MqttClient();

private:
  MqttClient();
  static MqttClient* _instance;

  void* _client; // esp_mqtt_client_handle_t
  char* _host;
  uint16_t _port;
  char* _path;        // WebSocket path (e.g., "/mqtt")
  bool _useWebSocket; // Transport over WebSocket
  bool _secure;       // mqtts/wss
  char* _uri;         // full URI if using URI-based config
  char* _username;
  char* _password;
  char* _clientId;
  uint16_t _keepalive;
  bool _connected;
  
  // TLS/mTLS certificate configuration
  char* _caCert;           // CA certificate for server verification
  char* _clientCert;       // Client certificate for mTLS
  char* _clientKey;        // Client private key for mTLS
  bool _skipCertVerify;    // Skip certificate verification (insecure mode)

  // Fallback configuration
  bool _enableFallback;
  bool _usingFallback;

  bool connectWithProtocol(esp_mqtt_protocol_ver_t protocol);
  void reconnectWithFallback();

  MessageCallback _messageCallback;
  SimpleCallback _connectCallback;
  SimpleCallback _disconnectCallback;

  void parseUriComponents(const char* uri);
  void handleMessage(const char* topic, const char* payload);
  void buildUriIfNeeded();

public:
  void onConnectedInternal();
  void onDisconnectedInternal();
  void onDataInternal(const char* topic, const char* data, int data_len);
};