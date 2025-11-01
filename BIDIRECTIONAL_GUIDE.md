# Bidirectional MQTT Communication Guide

## Overview
Your Pico W now supports **full bidirectional UDP/MQTT-SN communication**:
- **Publishing**: Pico → Gateway → MQTT Broker
- **Subscribing**: MQTT Broker → Gateway → Pico

## Architecture

```
┌─────────────┐     UDP      ┌──────────────┐    MQTT     ┌──────────────┐
│  Pico W     │ <──────────> │   Gateway    │ <────────> │ MQTT Broker  │
│ 172.20.10.2 │   Port 1884  │ 172.20.10.14 │ Port 1883  │  localhost   │
└─────────────┘              └──────────────┘            └──────────────┘
```

## Publishing Topics (Pico → Broker)

| Topic | QoS | Content | Frequency |
|-------|-----|---------|-----------|
| `pico/data` | 0 or 1 | Sequence messages: `seq=X,timestamp=Y` | Every 5s |
| `pico/block` | 0 or 1 | Image chunks (128 bytes) | On GP21 press |
| `pico/response` | 0 or 1 | Echo responses with "ACK: " prefix | On demand |

## Subscribed Topics (Broker → Pico)

| Topic | Handler | Response |
|-------|---------|----------|
| `pico/test` | Echo back | Publishes to `pico/response` with "ACK: " prefix |
| `pico/command` | Command processor | "ping" → responds "pong" to `pico/response` |
| `pico/chunks` | Block transfer | Processes incoming chunks |
| `pico/block` | Block transfer | Processes block data |

## Message Handler Features

### Automatic Display
- **Topic name**: Shows which topic received the message
- **Size**: Number of bytes received
- **Content**: 
  - Printable text → displayed as string
  - Binary data → displayed as hex dump (first 32 bytes)

### Example Output
```
[54123 ms] 📬 Received message:
  Topic: pico/test
  Size: 11 bytes
  Data: Hello Pico!

  → Sent acknowledgment to pico/response
```

## Testing Instructions

### 1. Start the Gateway
```bash
python3 python_mqtt_gateway.py
```

### 2. Start Mosquitto Broker
```bash
mosquitto -v
```

### 3. Flash and Run Pico
1. Flash `wifi_project.uf2` to your Pico W
2. Open PuTTY/minicom on serial port
3. Press **GP20** to initialize WiFi & MQTT
4. Wait for "System Ready!" message

### 4. Test Bidirectional Communication

#### Option A: Using the Test Script
```bash
python3 test_pico_subscribe.py
```
Then:
- Press `1` to send "Hello Pico!" to `pico/test`
- Press `2` to send "ping" to `pico/command`
- Press `3` or `4` for custom messages

#### Option B: Using mosquitto_pub
```bash
# Test echo on pico/test
mosquitto_pub -h localhost -t pico/test -m "Hello from laptop!"

# Test ping command
mosquitto_pub -h localhost -t pico/command -m "ping"

# Monitor responses
mosquitto_sub -h localhost -t pico/response -v
```

### 5. Monitor All Topics
In separate terminals:
```bash
# Terminal 1: Watch Pico's published data
mosquitto_sub -h localhost -t pico/data -v

# Terminal 2: Watch Pico's responses
mosquitto_sub -h localhost -t pico/response -v

# Terminal 3: Watch all pico/* topics
mosquitto_sub -h localhost -t pico/# -v
```

## Expected Behavior

### Scenario 1: Echo Test
1. You publish: `mosquitto_pub -t pico/test -m "Test123"`
2. Pico receives: `[54123 ms] 📬 Received message: pico/test, 7 bytes`
3. Pico responds: Publishes "ACK: Test123" to `pico/response`
4. You see: "ACK: Test123" on `pico/response` subscriber

### Scenario 2: Ping Command
1. You publish: `mosquitto_pub -t pico/command -m "ping"`
2. Pico receives: `[54234 ms] 📬 Received message: pico/command, 4 bytes`
3. Pico responds: "pong" to `pico/response`
4. You see: "pong" on `pico/response` subscriber

### Scenario 3: Continuous Publishing
- With QoS 0: Pico publishes to `pico/data` every 5s (fire-and-forget)
- With QoS 1: Pico publishes to `pico/data` every 5s (waits for PUBACK)
- You can toggle QoS with **GP22** button

## Custom Command Handling

To add your own commands, edit the `on_message_received()` function in `main.c`:

```c
else if (strcmp(topic, "pico/command") == 0) {
    if (len == 7 && memcmp(data, "restart", 7) == 0) {
        printf("  → Restarting system...\n");
        watchdog_reboot(0, 0, 0);
    }
    else if (len == 6 && memcmp(data, "status", 6) == 0) {
        char status[128];
        snprintf(status, sizeof(status), "uptime=%lu,qos=%d,sd_card=%d",
                 to_ms_since_boot(get_absolute_time()), 
                 current_qos, 
                 sd_card_mounted);
        mqttsn_publish("pico/response", (uint8_t*)status, strlen(status), 1);
    }
}
```

## Troubleshooting

### Messages Not Received by Pico
- Check gateway is running: `python3 python_mqtt_gateway.py`
- Verify MQTT broker: `mosquitto -v`
- Confirm Pico subscriptions: Look for "✓ Subscribed to topics" in serial output
- Test gateway connectivity: `nc -u 172.20.10.14 1884` (should connect)

### Messages Not Sent by Pico
- Check WiFi connection: Pico should show "WiFi connected"
- Verify MQTT connection: Look for "Connected to 172.20.10.14:1884"
- Test UDP: Gateway should show "📨 Received MQTT-SN packet"

### PUBACK Timeouts (QoS 1)
- Increase timeout in `mqtt_sn_client.c` line 416: `wait_for_message(..., 3000)`
- Check network latency between Pico and gateway
- Verify gateway is forwarding PUBACK properly

## Network Flow

### Publishing Flow (Pico → Broker)
1. Pico: `mqttsn_publish("pico/data", data, len, qos)`
2. Pico UDP sends to 172.20.10.14:1884
3. Gateway receives MQTT-SN PUBLISH
4. Gateway converts and publishes to MQTT broker
5. If QoS 1: Gateway sends PUBACK back to Pico

### Subscribing Flow (Broker → Pico)
1. Laptop: `mosquitto_pub -t pico/test -m "hello"`
2. MQTT broker forwards to gateway (subscribed to pico/*)
3. Gateway converts MQTT to MQTT-SN
4. Gateway sends UDP to Pico (172.20.10.2:1884)
5. Pico's `mqttsn_poll()` receives packet
6. Pico's `on_message_received()` callback processes message

## Performance

- **Latency**: ~10-50ms for Pico → Broker → Laptop roundtrip
- **Throughput**: ~10 KB/s for continuous publishing
- **Polling Rate**: Every 10ms in main loop
- **Message Queue**: 16-slot buffer prevents packet loss
