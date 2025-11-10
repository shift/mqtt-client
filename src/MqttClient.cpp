#include "MqttClient.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <cstring>

#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "mqtt_client.h"
#else
#include "esp_mqtt_client.h"
#endif
#else
#include "esp_mqtt_client.h"
#endif

// Define MQTT v5 constant if not available
#ifndef MQTT_PROTOCOL_V_5
#define MQTT_PROTOCOL_V_5 5
#endif

static const char* TAG = "MqttClient";

// Global event handler function
void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
  MqttClient* client = static_cast<MqttClient*>(handler_args);
  esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);

  switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED:
      Serial.printf("[MQTT] Connected to broker (session_present=%d)\n", event->session_present);
      client->onConnectedInternal();
      break;
    case MQTT_EVENT_DISCONNECTED:
      Serial.printf("[MQTT] Disconnected from broker (reason: %s)\n",
                    event->error_handle ? "error" : "clean disconnect");
      if (event->error_handle) {
        Serial.printf("[MQTT] Disconnect error type=%d, errno=%d\n",
                      event->error_handle->error_type,
                      event->error_handle->esp_transport_sock_errno);
      }
      client->onDisconnectedInternal();
      break;
    case MQTT_EVENT_SUBSCRIBED:
      Serial.printf("[MQTT] Subscribed, msg_id=%d\n", event->msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      Serial.printf("[MQTT] Unsubscribed, msg_id=%d\n", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      Serial.printf("[MQTT] Published, msg_id=%d\n", event->msg_id);
      break;
    case MQTT_EVENT_DATA: {
      char topic[256] = {0};
      char data[512] = {0};

      if (event->topic_len < sizeof(topic)) {
        memcpy(topic, event->topic, event->topic_len);
      }
      if (event->data_len < sizeof(data)) {
        memcpy(data, event->data, event->data_len);
      }

      Serial.printf("[MQTT] Message on %s: %s\n", topic, data);
      client->onDataInternal(topic, data, event->data_len);
    } break;
    case MQTT_EVENT_ERROR:
      Serial.println("[MQTT][ERROR] Error event details:");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        Serial.printf("  TCP transport error=%d, sock_errno=%d\n",
                      event->error_handle->esp_tls_last_esp_err,
                      event->error_handle->esp_transport_sock_errno);
      } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        Serial.printf("  Connection refused, return_code=%d\n", event->error_handle->connect_return_code);
        // MQTT v5 reason codes: 0x80=unspecified, 0x81=malformed, 0x82=protocol, etc.
        if (event->error_handle->connect_return_code >= 0x80) {
          Serial.println("  ^^ MQTT v5 reason code - check broker v5 support");
          switch (event->error_handle->connect_return_code) {
            case 0x80:
              Serial.println("  Reason: Unspecified error");
              break;
            case 0x81:
              Serial.println("  Reason: Malformed packet");
              break;
            case 0x82:
              Serial.println("  Reason: Protocol error");
              break;
            case 0x83:
              Serial.println("  Reason: Implementation specific error");
              break;
            case 0x84:
              Serial.println("  Reason: Unsupported protocol version");
              break;
            case 0x85:
              Serial.println("  Reason: Client identifier not valid");
              break;
            case 0x86:
              Serial.println("  Reason: Bad username or password");
              break;
            case 0x87:
              Serial.println("  Reason: Not authorized");
              break;
            case 0x88:
              Serial.println("  Reason: Server unavailable");
              break;
            case 0x89:
              Serial.println("  Reason: Server busy");
              break;
            case 0x8A:
              Serial.println("  Reason: Banned");
              break;
            default:
              Serial.printf("  Reason: Unknown v5 code 0x%02X\n", event->error_handle->connect_return_code);
              break;
          }
        }
      } else {
        Serial.printf("  Unknown error type=%d\n", event->error_handle->error_type);
      }
      break;
    default:
      Serial.printf("[MQTT] Unknown event id:%d\n", event_id);
      break;
  }
}

MqttClient* MqttClient::_instance = nullptr;

MqttClient::MqttClient()
    : _client(nullptr),
      _host(nullptr),
      _port(1883),
  _path(nullptr),
  _useWebSocket(false),
  _secure(false),
  _uri(nullptr),
      _username(nullptr),
      _password(nullptr),
      _clientId(nullptr),
      _keepalive(30),
      _connected(false),
      _caCert(nullptr),
      _clientCert(nullptr),
      _clientKey(nullptr),
      _skipCertVerify(false),
      _enableFallback(false),
      _usingFallback(false) {
}

MqttClient* MqttClient::getInstance() {
  if (_instance == nullptr) {
    _instance = new MqttClient();
  }
  return _instance;
}

MqttClient::~MqttClient() {
  if (_client) {
    esp_mqtt_client_stop(static_cast<esp_mqtt_client_handle_t>(_client));
    esp_mqtt_client_destroy(static_cast<esp_mqtt_client_handle_t>(_client));
  }
  free(_host);
  free(_path);
  free(_uri);
  free(_username);
  free(_password);
  free(_clientId);
  free(_caCert);
  free(_clientCert);
  free(_clientKey);
}

void MqttClient::parseUriComponents(const char* uri) {
  // Parse scheme, host, port, and optional path for ws/wss
  UriParts parts;
  if (!parseMqttUri(std::string(uri), parts)) {
    Serial.println("[MQTT][ERROR] Invalid broker URI");
    return;
  }

  // Store host and port
  _host = static_cast<char*>(realloc(_host, parts.host.size() + 1));
  memcpy(_host, parts.host.c_str(), parts.host.size() + 1);
  _port = parts.port;

  // Transport flags
  _useWebSocket = parts.isWebSocket();
  _secure = parts.isSecure();

  // Path for WebSocket
  if (_useWebSocket) {
    _path = static_cast<char*>(realloc(_path, parts.path.size() + 1));
    memcpy(_path, parts.path.c_str(), parts.path.size() + 1);
  }
}

void MqttClient::begin(const char* brokerUri) {
  parseUriComponents(brokerUri);
}

void MqttClient::setServer(const char* host, uint16_t port) {
  _host = static_cast<char*>(realloc(_host, strlen(host) + 1));
  strcpy(_host, host);
  _port = port;
}

void MqttClient::setWebSocket(bool enable) {
  _useWebSocket = enable;
}

void MqttClient::setPath(const char* path) {
  if (!path) return;
  size_t len = strlen(path);
  _path = static_cast<char*>(realloc(_path, len + 1));
  strcpy(_path, path);
}

void MqttClient::setCredentials(const char* username, const char* password) {
  if (username) {
    _username = static_cast<char*>(realloc(_username, strlen(username) + 1));
    strcpy(_username, username);
  }
  if (password) {
    _password = static_cast<char*>(realloc(_password, strlen(password) + 1));
    strcpy(_password, password);
  }
}

void MqttClient::setKeepalive(uint16_t keepalive) {
  _keepalive = keepalive;
}

void MqttClient::setCACert(const char* ca_cert) {
  if (ca_cert) {
    size_t len = strlen(ca_cert);
    _caCert = static_cast<char*>(realloc(_caCert, len + 1));
    strcpy(_caCert, ca_cert);
    Serial.println("[MQTT][INFO] CA certificate configured");
  }
}

void MqttClient::setClientCert(const char* client_cert) {
  if (client_cert) {
    size_t len = strlen(client_cert);
    _clientCert = static_cast<char*>(realloc(_clientCert, len + 1));
    strcpy(_clientCert, client_cert);
    Serial.println("[MQTT][INFO] Client certificate configured for mTLS");
  }
}

void MqttClient::setClientKey(const char* client_key) {
  if (client_key) {
    size_t len = strlen(client_key);
    _clientKey = static_cast<char*>(realloc(_clientKey, len + 1));
    strcpy(_clientKey, client_key);
    Serial.println("[MQTT][INFO] Client private key configured for mTLS");
  }
}

void MqttClient::setInsecure(bool insecure) {
  _skipCertVerify = insecure;
  Serial.print("[MQTT][WARNING] Certificate verification ");
  Serial.println(insecure ? "DISABLED (insecure mode)" : "enabled");
}

void MqttClient::setProtocolFallback(bool enableFallback) {
  _enableFallback = enableFallback;
  Serial.print("[MQTT][INFO] Protocol fallback ");
  Serial.println(enableFallback ? "enabled" : "disabled");
}

bool MqttClient::connect(const char* clientId) {
  _clientId = static_cast<char*>(realloc(_clientId, strlen(clientId) + 1));
  strcpy(_clientId, clientId);

  // Validate client ID is not empty
  if (!clientId || strlen(clientId) == 0) {
    Serial.println("[MQTT][ERROR] Client ID cannot be empty");
    return false;
  }

  Serial.print("[MQTT][INFO] Attempting connection with client ID: ");
  Serial.println(clientId);

  // Try MQTT v5 first
  Serial.println("[MQTT][INFO] Attempting MQTT v5 connection...");
  if (connectWithProtocol(static_cast<esp_mqtt_protocol_ver_t>(5))) {
    _usingFallback = false;
    Serial.println("[MQTT][SUCCESS] Connected using MQTT v5");
    return true;
  }

  // If v5 fails and fallback is enabled, try v3.1.1
  if (_enableFallback) {
    Serial.println("[MQTT][INFO] MQTT v5 failed, attempting fallback to v3.1.1...");
    if (connectWithProtocol(static_cast<esp_mqtt_protocol_ver_t>(4))) {
      _usingFallback = true;
      Serial.println("[MQTT][SUCCESS] Connected using MQTT v3.1.1 fallback");
      return true;
    }
  }

  Serial.println("[MQTT][ERROR] All connection attempts failed");
  return false;
}

bool MqttClient::connectWithProtocol(esp_mqtt_protocol_ver_t protocol) {
  const char* protocolName = (protocol == MQTT_PROTOCOL_V_5) ? "v5" : "v3.1.1";
  Serial.print("[MQTT][INFO] Configuring for MQTT ");
  Serial.println(protocolName);

  // Build a URI if using WebSocket or when a path is specified
  buildUriIfNeeded();

#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_mqtt_client_config_t mqtt_cfg = {
      .broker =
          {
              .address =
                  {
                      // Prefer URI when using WebSocket or when explicitly built
                      .uri = _uri ? _uri : nullptr,
                      .hostname = _uri ? nullptr : _host,
                      .port = _uri ? 0 : _port,
                  },
              .verification =
                  {
                      .certificate = _caCert,
                      .skip_cert_common_name_check = _skipCertVerify,
                  },
          },
      .credentials =
          {
              .client_id = _clientId,
              .username = _username,
              .authentication =
                  {
                      .password = _password,
                      .certificate = _clientCert,
                      .key = _clientKey,
                  },
          },
      .session =
          {
              .keepalive = _keepalive,
              .protocol_ver = protocol,
          },
  };
#else
  esp_mqtt_client_config_t mqtt_cfg = {};
  if (_uri) {
    mqtt_cfg.uri = _uri;
  } else {
    mqtt_cfg.host = _host;
    mqtt_cfg.port = _port;
  }
  mqtt_cfg.client_id = _clientId;
  mqtt_cfg.username = _username;
  mqtt_cfg.password = _password;
  mqtt_cfg.keepalive = _keepalive;
  mqtt_cfg.protocol_ver = protocol;
  
  // TLS/mTLS configuration for IDF < 5.0
  mqtt_cfg.cert_pem = _caCert;
  mqtt_cfg.client_cert_pem = _clientCert;
  mqtt_cfg.client_key_pem = _clientKey;
  mqtt_cfg.skip_cert_common_name_check = _skipCertVerify;
#endif
#else
  esp_mqtt_client_config_t mqtt_cfg = {};
  if (_uri) {
    mqtt_cfg.uri = _uri;
  } else {
    mqtt_cfg.host = _host;
    mqtt_cfg.port = _port;
  }
  mqtt_cfg.client_id = _clientId;
  mqtt_cfg.username = _username;
  mqtt_cfg.password = _password;
  mqtt_cfg.keepalive = _keepalive;
  
  // TLS/mTLS configuration for older ESP-IDF
  mqtt_cfg.cert_pem = _caCert;
  mqtt_cfg.client_cert_pem = _clientCert;
  mqtt_cfg.client_key_pem = _clientKey;
  mqtt_cfg.skip_cert_common_name_check = _skipCertVerify;
  // For older ESP-IDF versions, protocol selection may not be available
#endif

  if (_client) {
    esp_mqtt_client_stop(static_cast<esp_mqtt_client_handle_t>(_client));
    esp_mqtt_client_destroy(static_cast<esp_mqtt_client_handle_t>(_client));
  }

  _client = esp_mqtt_client_init(&mqtt_cfg);
  if (!_client) {
    Serial.print("[MQTT][ERROR] Failed to initialize client for ");
    Serial.println(protocolName);
    return false;
  }

  esp_mqtt_client_register_event(static_cast<esp_mqtt_client_handle_t>(_client),
                                 static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                 mqtt_event_handler,
                                 this);

  if (_uri) {
    Serial.printf("[MQTT] Connecting to %s as %s (%s)\n", _uri, _clientId, protocolName);
  } else {
    Serial.printf("[MQTT] Connecting to %s:%d as %s (%s)\n", _host, _port, _clientId, protocolName);
  }
  Serial.printf("[MQTT] Config: keepalive=%ds, username=%s, password=%s\n",
                _keepalive,
                _username ? _username : "(none)",
                _password ? "***" : "(none)");

  esp_err_t result = esp_mqtt_client_start(static_cast<esp_mqtt_client_handle_t>(_client));
  if (result == ESP_OK) {
    Serial.print("[MQTT][INFO] Client started successfully for ");
    Serial.println(protocolName);
    return true;
  } else {
    Serial.print("[MQTT][ERROR] Failed to start client for ");
    Serial.print(protocolName);
    Serial.print(", error: ");
    Serial.println(result);
    return false;
  }
}

void MqttClient::disconnect() {
  if (_client) {
    esp_mqtt_client_disconnect(static_cast<esp_mqtt_client_handle_t>(_client));
  }
}

int MqttClient::publish(const char* topic, const char* payload, bool retain) {
  if (!_client)
    return -1;

  int msg_id =
      esp_mqtt_client_publish(static_cast<esp_mqtt_client_handle_t>(_client), topic, payload, 0, 1, retain ? 1 : 0);
  return msg_id;
}

int MqttClient::subscribe(const char* topic, int qos) {
  if (!_client)
    return -1;

  int msg_id = esp_mqtt_client_subscribe(static_cast<esp_mqtt_client_handle_t>(_client), topic, qos);
  return msg_id;
}

int MqttClient::unsubscribe(const char* topic) {
  if (!_client)
    return -1;

  int msg_id = esp_mqtt_client_unsubscribe(static_cast<esp_mqtt_client_handle_t>(_client), topic);
  return msg_id;
}

void MqttClient::onMessage(MessageCallback cb) {
  _messageCallback = cb;
}

void MqttClient::onConnect(SimpleCallback cb) {
  _connectCallback = cb;
}

void MqttClient::onDisconnect(SimpleCallback cb) {
  _disconnectCallback = cb;
}

bool MqttClient::isConnected() const {
  return _connected && _client != nullptr;
}

void MqttClient::handleMessage(const char* topic, const char* payload) {
  if (_messageCallback) {
    _messageCallback(topic, payload, strlen(payload));
  }
}

void MqttClient::onConnectedInternal() {
  _connected = true;
  unsigned long connect_time = millis();
  Serial.printf("[MQTT] Connected at %lu ms - connection state: true\n", connect_time);
  if (_connectCallback) {
    _connectCallback();
  }
}

void MqttClient::onDisconnectedInternal() {
  unsigned long disconnect_time = millis();
  Serial.printf("[MQTT] Disconnected at %lu ms - connection state: false\n", disconnect_time);

  bool wasConnected = _connected;
  _connected = false;

  // If we were previously connected and fallback is enabled, try fallback
  if (wasConnected && _enableFallback && !_usingFallback) {
    Serial.println("[MQTT][INFO] Disconnected, attempting fallback to v3.1.1...");
    reconnectWithFallback();
    return;
  }

  if (_disconnectCallback) {
    _disconnectCallback();
  }
}

void MqttClient::reconnectWithFallback() {
  Serial.println("[MQTT][INFO] Attempting reconnection with fallback...");

  // Small delay before reconnection attempt
  delay(1000);

  // Try v3.1.1 fallback
  if (connectWithProtocol(static_cast<esp_mqtt_protocol_ver_t>(4))) {
    _usingFallback = true;
    Serial.println("[MQTT][SUCCESS] Reconnected using MQTT v3.1.1 fallback");
  } else {
    Serial.println("[MQTT][ERROR] Fallback reconnection failed");
    if (_disconnectCallback) {
      _disconnectCallback();
    }
  }
}

void MqttClient::onDataInternal(const char* topic, const char* data, int data_len) {
  if (_messageCallback) {
    _messageCallback(topic, data, data_len);
  }
}
void MqttClient::loop() {
  // No-op: esp-mqtt is event-driven
}

void MqttClient::buildUriIfNeeded() {
  // Free previous URI if any
  if (_uri) {
    free(_uri);
    _uri = nullptr;
  }

  if (_useWebSocket || (_path && *_path)) {
    // Build scheme
    UriParts parts;
    parts.scheme = _secure ? (_useWebSocket ? "wss" : "mqtts") : (_useWebSocket ? "ws" : "mqtt");
    parts.host = _host ? _host : "";
    parts.port = _port;
    parts.path = _useWebSocket ? (_path ? _path : "/") : std::string();

    std::string uri = buildMqttUri(parts);
    _uri = static_cast<char*>(malloc(uri.size() + 1));
    strcpy(_uri, uri.c_str());
  }
}