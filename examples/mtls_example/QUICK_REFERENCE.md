# mTLS Quick Reference Guide

## Basic mTLS Setup

```cpp
#include "MqttClient.h"

// Get client instance
MqttClient* mqtt = MqttClient::getInstance();

// Configure broker
mqtt->begin("mqtts://broker.example.com:8883");
mqtt->setCredentials("username", "password");

// Configure certificates
mqtt->setCACert(ca_cert_pem);       // Server verification
mqtt->setClientCert(client_cert_pem); // Client authentication
mqtt->setClientKey(client_key_pem);   // Client private key

// Connect
mqtt->connect("my-device-id");
```

## Certificate Formats

All certificates must be in PEM format as C strings:

```cpp
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
...
-----END CERTIFICATE-----
)EOF";
```

## Common Configurations

### AWS IoT Core

```cpp
mqtt->begin("mqtts://your-endpoint.iot.region.amazonaws.com:8883");
mqtt->setCACert(amazon_root_ca_1);
mqtt->setClientCert(device_certificate);
mqtt->setClientKey(device_private_key);
mqtt->connect("thing-name");
```

### Azure IoT Hub

```cpp
mqtt->begin("mqtts://your-hub.azure-devices.net:8883");
mqtt->setCACert(digicert_baltimore_root);
mqtt->setClientCert(device_cert);
mqtt->setClientKey(device_key);
mqtt->setCredentials("your-hub.azure-devices.net/device-id/?api-version=2021-04-12", sas_token);
mqtt->connect("device-id");
```

### Google Cloud IoT Core

```cpp
mqtt->begin("mqtts://mqtt.googleapis.com:8883");
mqtt->setCACert(google_roots_pem);
mqtt->setClientCert(device_cert);
mqtt->setClientKey(device_key);
mqtt->connect("projects/PROJECT_ID/locations/REGION/registries/REGISTRY_ID/devices/DEVICE_ID");
```

### Self-Hosted Mosquitto

```cpp
mqtt->begin("mqtts://your-server.com:8883");
mqtt->setCACert(your_ca_cert);
mqtt->setClientCert(client_cert);
mqtt->setClientKey(client_key);
mqtt->connect("client-id");
```

## API Methods

| Method | Description |
|--------|-------------|
| `setCACert(const char*)` | Set CA certificate for server verification |
| `setClientCert(const char*)` | Set client certificate for mTLS |
| `setClientKey(const char*)` | Set client private key for mTLS |
| `setInsecure(bool)` | Skip cert verification (testing only) |

## Security Levels

### Level 1: No Encryption (Insecure)
```cpp
mqtt->begin("mqtt://broker:1883");  // ❌ Not recommended
```

### Level 2: TLS (Server Authentication)
```cpp
mqtt->begin("mqtts://broker:8883");
mqtt->setCACert(ca_cert);  // ✓ Encrypted, server verified
```

### Level 3: mTLS (Mutual Authentication)
```cpp
mqtt->begin("mqtts://broker:8883");
mqtt->setCACert(ca_cert);
mqtt->setClientCert(client_cert);
mqtt->setClientKey(client_key);  // ✓✓ Encrypted, both verified
```

## Troubleshooting Commands

### Test Server Certificate
```bash
openssl s_client -connect broker.example.com:8883 -CAfile ca.crt
```

### Test Client Certificate
```bash
openssl s_client -connect broker.example.com:8883 \
  -CAfile ca.crt \
  -cert client.crt \
  -key client.key
```

### Verify Certificate Details
```bash
openssl x509 -in certificate.crt -text -noout
```

### Check Certificate Expiry
```bash
openssl x509 -in certificate.crt -noout -dates
```

## Error Messages

| Error | Likely Cause | Solution |
|-------|--------------|----------|
| `TCP transport error` | Network/TLS issue | Check certificates, CA chain |
| `Connection refused 0x86` | Bad credentials | Verify username/password |
| `Connection refused 0x87` | Not authorized | Check certificate permissions/policy |
| `Connection refused 0x82` | Protocol error | Check MQTT version compatibility |
| `esp_tls_last_esp_err` | Certificate validation failed | Verify CA cert, check expiry |

## Memory Considerations

Typical certificate sizes:
- CA certificate: ~1-2 KB
- Client certificate: ~1-2 KB  
- Private key (RSA 2048): ~1.7 KB
- **Total: ~5 KB**

**Platform Requirements:**
- **ESP32**: ✅ Sufficient memory for mTLS (recommended minimum 200KB free heap)
- **ESP8266**: ❌ Not supported - This library uses ESP-IDF APIs which are ESP32-only

The ESP8266 has very limited RAM (~80KB heap) and uses a different MQTT client implementation that is not compatible with this library.

## Best Practices Checklist

- [ ] Use unique certificates per device
- [ ] Store private keys securely (not hardcoded)
- [ ] Set certificate expiry monitoring
- [ ] Use minimum 2048-bit RSA keys
- [ ] Never commit private keys to version control
- [ ] Test certificate expiry before deployment
- [ ] Use `setInsecure(true)` only for testing
- [ ] Implement certificate rotation strategy
- [ ] Log certificate-related errors
- [ ] Monitor connection success rates

## Certificate Rotation

```cpp
// Before certificate expires, provision new certificates
void updateCertificates() {
  mqtt->disconnect();
  
  mqtt->setCACert(new_ca_cert);
  mqtt->setClientCert(new_client_cert);
  mqtt->setClientKey(new_client_key);
  
  mqtt->connect("device-id");
}
```

## Performance Tips

- Reuse TLS sessions when possible
- Use shorter certificate chains
- Consider ECC certificates (smaller, faster)
- Cache CA certificates in flash
- Use hardware crypto acceleration if available

## Additional Resources

- [Library README](../../README.md)
- [Full mTLS Example](./README.md)
- [AWS IoT Docs](https://docs.aws.amazon.com/iot/)
- [ESP-IDF TLS Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_tls.html)
