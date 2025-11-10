# mTLS (Mutual TLS) MQTT Client Example

This example demonstrates how to use the MQTT client library with mutual TLS authentication, where both the client and server verify each other's certificates.

## What is mTLS?

Mutual TLS (mTLS) is a security protocol that provides two-way authentication:

1. **Server Authentication**: The client verifies the server's certificate (standard TLS)
2. **Client Authentication**: The server verifies the client's certificate (the "mutual" part)

This is more secure than username/password authentication alone, as it requires possession of a valid private key and certificate.

## Use Cases

- **IoT Device Authentication**: Securely authenticate IoT devices to cloud platforms (AWS IoT, Azure IoT Hub, Google Cloud IoT)
- **High-Security Applications**: Financial services, healthcare, industrial control systems
- **Zero-Trust Networks**: Where every connection must be authenticated with certificates
- **API Security**: Machine-to-machine communication requiring strong authentication

## Prerequisites

Before running this example, you need to obtain three certificates:

### 1. CA Certificate (`ca_cert`)

The Certificate Authority certificate used to verify the broker's certificate.

- For public brokers, use their published CA certificate
- For AWS IoT, download the Amazon Root CA certificate
- For self-signed certificates, use your own CA certificate

### 2. Client Certificate (`client_cert`)

Your device's public certificate for authentication.

- Must be signed by a CA that the broker trusts
- Contains your device's public key and identity information
- Safe to share (it's public)

### 3. Client Private Key (`client_key`)

Your device's private key that corresponds to the client certificate.

- **MUST BE KEPT SECRET**
- Never commit to version control
- Never share or transmit insecurely
- Should be unique per device

## Obtaining Certificates

### AWS IoT Core

1. Create a Thing in AWS IoT Console
2. Create and activate a certificate
3. Download:
   - Device certificate → `client_cert`
   - Private key → `client_key`
   - Amazon Root CA 1 → `ca_cert`
4. Attach a policy to the certificate with appropriate permissions

### Self-Signed Certificates (Testing)

Generate your own certificates for testing:

```bash
# Generate CA key and certificate
openssl genrsa -out ca.key 2048
openssl req -x509 -new -nodes -key ca.key -sha256 -days 1024 -out ca.crt

# Generate client key and certificate signing request
openssl genrsa -out client.key 2048
openssl req -new -key client.key -out client.csr

# Sign the client certificate with the CA
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365 -sha256

# Convert to PEM format if needed
cat ca.crt       # Copy this as ca_cert
cat client.crt   # Copy this as client_cert
cat client.key   # Copy this as client_key
```

### Using Let's Encrypt or Other Public CAs

For production deployments with domain names, you can use Let's Encrypt or commercial CAs to obtain trusted certificates.

## Configuration

1. **Update WiFi Credentials**:
   ```cpp
   const char *ssid = "your_wifi_ssid";
   const char *password = "your_wifi_password";
   ```

2. **Update Broker Settings**:
   ```cpp
   const char *mqtt_broker = "mqtts://your-broker.example.com:8883";
   const char *mqtt_username = "your_username";  // May be optional with mTLS
   const char *mqtt_password = "your_password";  // May be optional with mTLS
   ```

3. **Update Certificates**:
   - Replace `ca_cert` with your broker's CA certificate
   - Replace `client_cert` with your device certificate
   - Replace `client_key` with your device private key

   **Important**: Keep certificates as multi-line strings with proper PEM formatting:
   ```cpp
   const char* ca_cert = R"EOF(
   -----BEGIN CERTIFICATE-----
   MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w...
   ...
   -----END CERTIFICATE-----
   )EOF";
   ```

## How It Works

### Connection Flow

1. **WiFi Connection**: Connects to the configured WiFi network
2. **Certificate Configuration**: Loads CA cert, client cert, and client key
3. **TLS Handshake**:
   - Client verifies server's certificate using CA cert
   - Server requests client certificate
   - Client presents its certificate and signs with private key
   - Server verifies client certificate
4. **MQTT Connection**: Establishes MQTT session over secured TLS connection
5. **Subscribe & Publish**: Normal MQTT operations over encrypted channel

### Key Functions

```cpp
// Set CA certificate for server verification
mqtt->setCACert(ca_cert);

// Set client certificate for authentication
mqtt->setClientCert(client_cert);

// Set client private key
mqtt->setClientKey(client_key);

// Optional: Skip verification for testing (INSECURE!)
mqtt->setInsecure(true);
```

## Security Best Practices

### ✅ DO

- **Store private keys securely**: Use secure storage or external crypto chips (like ATECC608)
- **Use unique certificates per device**: Never share certificates between devices
- **Rotate certificates regularly**: Implement certificate expiry and renewal
- **Use strong key lengths**: Minimum 2048-bit RSA or 256-bit ECC
- **Keep firmware updated**: Ensure TLS libraries are current
- **Monitor certificate expiry**: Set up alerts before certificates expire

### ❌ DON'T

- **Never commit private keys to Git**: Use `.gitignore` or environment variables
- **Never disable verification in production**: Only use `setInsecure(true)` for testing
- **Don't use self-signed certs in production**: Use proper CA-signed certificates
- **Don't use weak encryption**: Avoid deprecated TLS versions and cipher suites
- **Don't hardcode secrets**: Consider using secure provisioning methods

## Troubleshooting

### Connection Refused

- **Check certificate validity**: Ensure certificates haven't expired
- **Verify CA chain**: Make sure the CA cert can verify the server's cert
- **Check client cert**: Ensure it's signed by a CA the broker trusts
- **Review broker logs**: Check server-side for certificate validation errors

### TLS Handshake Failed

- **Verify PEM format**: Certificates must be properly formatted
- **Check private key**: Must match the client certificate
- **Ensure correct CA**: Must be the CA that signed the server's certificate
- **Check TLS version**: Ensure broker and client support compatible TLS versions

### Authentication Failed

- **Verify certificate policies**: Check if broker has proper ACLs for the certificate
- **Check username/password**: Some brokers require both cert and credentials
- **Review certificate CN/SAN**: Common Name or Subject Alternative Name must match requirements
- **Check certificate permissions**: Ensure the cert has permission to publish/subscribe

### Memory Issues

- **Certificate size**: Large certificate chains can cause memory issues on ESP32
- **Reduce certificate chain**: Only include necessary intermediate certificates
- **Increase partition size**: Adjust flash partition layout if needed
- **Platform limitation**: This library requires ESP32 - ESP8266 is not supported due to ESP-IDF dependency

## Monitoring

The example includes comprehensive logging:

```
[MQTT][INFO] CA certificate configured
[MQTT][INFO] Client certificate configured for mTLS
[MQTT][INFO] Client private key configured for mTLS
[MQTT] Connecting to mqtts://broker.example.com:8883 as esp32_mtls_client (v5)
[MQTT] Connected to broker (session_present=0)
✓ Successfully connected to MQTT broker with mTLS!
```

## Testing Without a Broker

For local testing, you can set up a Mosquitto broker with mTLS:

```bash
# Install Mosquitto
sudo apt-get install mosquitto mosquitto-clients

# Configure mosquitto.conf for mTLS
listener 8883
cafile /path/to/ca.crt
certfile /path/to/server.crt
keyfile /path/to/server.key
require_certificate true
use_identity_as_username true

# Start broker
mosquitto -c mosquitto.conf -v
```

## Additional Resources

- [AWS IoT Core Certificate Configuration](https://docs.aws.amazon.com/iot/latest/developerguide/x509-client-certs.html)
- [ESP-IDF MQTT over TLS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mqtt.html)
- [OpenSSL Certificate Commands](https://www.openssl.org/docs/man1.1.1/man1/openssl.html)
- [Mosquitto TLS Configuration](https://mosquitto.org/man/mosquitto-tls-7.html)

## License

This example is provided under the same license as the main library (MIT License).
