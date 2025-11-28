# Automated Integration Tests

This directory contains automated and semi-automated integration tests for the MQTT-SN Pico W project.

## Overview

Tests have been converted from fully manual to automated or semi-automated:

| Test | Type | Description | Automation Level |
|------|------|-------------|------------------|
| IT1  | Auto | QoS 0 Message Delivery | ✅ Fully Automated |
| IT2  | Auto | QoS 1 with PUBACK | ✅ Fully Automated |
| IT3  | Semi | QoS 1 Retry with DUP Flag | 🔄 Semi-Automated (Gateway control) |
| IT4  | Semi | QoS 1 Dropped Connection | 🔄 Semi-Automated (Unplug/replug) |
| IT5  | Auto | Block Transfer | ✅ Fully Automated (existing) |
| IT6  | Semi | Block Transfer Fragment Loss | 🔄 Semi-Automated (Gateway control) |
| IT7  | Semi | Topic Registration | 🔄 Semi-Automated (Pico reset) |
| IT8  | Auto | Topic Subscription | ✅ Fully Automated |
| IT9  | Semi | WiFi Auto-Reconnect | 🔄 Semi-Automated (WiFi on/off) |
| IT10 | Semi | Topic Registration After Disconnect | 🔄 Semi-Automated (Gateway control) |
| IT11 | Semi | WiFi Connection on Boot | 🔄 Semi-Automated (Pico reset) |
| IT12 | Semi | SD Card Operations | 🔄 Semi-Automated (Button press) |

## Quick Start

### Prerequisites

1. **Python Dependencies:**
   ```bash
   pip install paho-mqtt
   ```

2. **WSL Services Running:**
   ```bash
   # Mosquitto Broker
   sudo systemctl start mosquitto
   sudo systemctl status mosquitto
   
   # MQTT-SN Gateway
   cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
   ./MQTT-SNGateway
   ```

3. **Hardware:**
   - Pico W connected via USB
   - SD card inserted (for IT12)
   - Serial monitor open for verification

### Running Tests

#### Run All Tests
```bash
python run_automated_tests.py all
```

#### Run Only Fully Automated Tests
```bash
python run_automated_tests.py auto
```
These run without any manual intervention (IT1, IT2, IT8).

#### Run Only Semi-Automated Tests
```bash
python run_automated_tests.py semi
```
These require manual actions but have automated verification (IT3, IT4, IT6, IT7, IT9, IT10, IT11, IT12).

#### Run Individual Tests
```bash
# Fully automated
python test_it1_qos0_message_delivery.py
python test_it2_qos1_message_delivery.py
python test_it8_topic_subscription.py

# Semi-automated
python test_it3_qos1_retry_automated.py
python test_it4_qos1_dropped_connection_automated.py
python test_it6_block_transfer_fragment_loss_automated.py
python test_it7_topic_registration_automated.py
python test_it9_wifi_auto_reconnect_automated.py
python test_it10_topic_registration_after_disconnect_automated.py
python test_it11_wifi_connection_automated.py
python test_it12_sd_card_operations_automated.py
```

## Test Details

### Fully Automated Tests

These tests run completely automatically with no manual intervention:

#### IT1: QoS 0 Message Delivery
- **File:** `test_it1_qos0_message_delivery.py`
- **Duration:** ~1 minute (10 test cases)
- **Prerequisites:** Pico set to QoS 0 (press GP22)

#### IT2: QoS 1 Message Delivery
- **File:** `test_it2_qos1_message_delivery.py`
- **Duration:** ~1 minute (10 test cases)
- **Prerequisites:** Pico set to QoS 1 (press GP22)

#### IT8: Topic Subscription
- **File:** `test_it8_topic_subscription.py`
- **Duration:** ~30 seconds (50 test cases)
- **Prerequisites:** Broker running

### Semi-Automated Tests

These tests automate the verification but require manual actions:

#### IT3: QoS 1 Retry with DUP Flag
- **File:** `test_it3_qos1_retry_automated.py`
- **Manual Actions:**
  - Test automatically stops/starts MQTT-SN Gateway via WSL
  - Monitor serial output for DUP=1 flag verification
- **Duration:** ~2 minutes per test (10 test cases = ~20 minutes)

#### IT4: QoS 1 Dropped Connection
- **File:** `test_it4_qos1_dropped_connection_automated.py`
- **Manual Actions:**
  - Unplug and replug Pico USB cable when prompted
- **Duration:** ~2 minutes per test (10 test cases = ~20 minutes)

#### IT6: Block Transfer Fragment Loss
- **File:** `test_it6_block_transfer_fragment_loss_automated.py`
- **Manual Actions:**
  - Press GP21 to start transfer when prompted
  - Test automatically disrupts gateway to simulate loss
- **Duration:** ~2 minutes per test (10 test cases = ~20 minutes)

#### IT7: Topic Registration
- **File:** `test_it7_topic_registration_automated.py`
- **Manual Actions:**
  - Reset Pico W (unplug/replug) when prompted
- **Duration:** ~1 minute per test (50 test cases = ~50 minutes)
- **Note:** Can reduce TEST_RUNS to 10 for faster testing

#### IT9: WiFi Auto-Reconnect
- **File:** `test_it9_wifi_auto_reconnect_automated.py`
- **Manual Actions:**
  - Turn WiFi router/access point OFF when prompted
  - Turn WiFi router/access point ON when prompted
- **Duration:** ~2 minutes per test (10 test cases = ~20 minutes)

#### IT10: Topic Registration After Disconnect
- **File:** `test_it10_topic_registration_after_disconnect_automated.py`
- **Manual Actions:**
  - Test automatically stops/starts MQTT-SN Gateway via WSL
  - Monitor serial output for verification
- **Duration:** ~1 minute per test (10 test cases = ~10 minutes)

#### IT11: WiFi Connection on Boot
- **File:** `test_it11_wifi_connection_automated.py`
- **Manual Actions:**
  - Reset Pico W (unplug/replug) when prompted
- **Duration:** ~30 seconds per test (10 test cases = ~5 minutes)

#### IT12: SD Card Operations
- **File:** `test_it12_sd_card_operations_automated.py`
- **Manual Actions:**
  - Press GP21 to start transfer when prompted
- **Prerequisites:** SD card with image file inserted
- **Duration:** ~1 minute per test (10 test cases = ~10 minutes)

## Test Output Format

All automated tests output results in this format:

```
============================================================
TEST SUMMARY
============================================================
Passed: X/Y
Failed: Z/Y
Success Rate: W%

Tolerance: X out of Y test cases, achieved W% success
```

**Copy the "Tolerance" line into test2.txt for documentation.**

## Adjusting Test Runs

To run more or fewer iterations, edit the `TEST_RUNS` variable in each test file:

```python
# Default for most tests
TEST_RUNS = 10

# Change to run more tests
TEST_RUNS = 50  # or 100 for comprehensive testing
```

**Quick Testing (Default):**
- 10 iterations per test
- Total time: ~2 hours for all tests

**Comprehensive Testing:**
- 50-100 iterations per test
- Total time: ~5-10 hours for all tests

## Automation Features

### Gateway Control (IT3, IT6, IT10)
Tests automatically control the MQTT-SN Gateway via WSL:
- Stop gateway: `wsl pkill -f MQTT-SNGateway`
- Start gateway: `wsl bash -c "cd ~/path && ./MQTT-SNGateway &"`

### Message Monitoring
Tests monitor MQTT broker for:
- Message delivery and timing
- Duplicate detection
- Topic registration verification
- Fragment reception and retransmission

### Statistical Analysis
Tests automatically calculate:
- Success/failure rates
- Timing measurements
- Duplicate message detection
- Fragment loss and recovery

## Troubleshooting

### "No module named paho"
```bash
pip install paho-mqtt
```

### "Connection failed"
Check services:
```bash
sudo systemctl status mosquitto
```

### "Could not control gateway"
Verify WSL gateway path in test scripts:
```python
# Update this path if needed
subprocess.Popen(["wsl", "bash", "-c", "cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin && ./MQTT-SNGateway &"])
```

### "No messages received"
- Check Pico W is running (serial monitor)
- Verify QoS level (press GP22 to toggle)
- Ensure gateway is running
- Check WiFi connection

### Tests Run Too Slowly
Reduce `TEST_RUNS` in individual test files:
```python
TEST_RUNS = 5  # Instead of 10 or 50
```

## Serial Verification

While tests automate the MQTT broker side verification, always check the Pico serial output for:

- **IT3:** DUP=1 flag on retransmission
- **IT4:** Reconnection messages and DUP=1
- **IT6:** NACK and retransmission logs
- **IT7:** Topic registration timing (<800ms each)
- **IT9:** WiFi disconnect/reconnect messages
- **IT10:** Topic re-registration after disconnect
- **IT11:** WiFi connection timing during boot
- **IT12:** SD card initialization and read success

## Test Results Documentation

After running tests, update `test2.txt` with the tolerance line from each test output.

Example:
```
IT3: QoS 1 - Sender resend message after timeout
Tolerance: 8 out of 10 test cases, achieved 80% success
```

## Tips for Efficient Testing

1. **Run automated tests first** (IT1, IT2, IT8) to verify basic functionality
2. **Group similar tests** (e.g., all gateway control tests together)
3. **Use master runner** for consistent test sequencing
4. **Monitor serial output** throughout for immediate error detection
5. **Adjust TEST_RUNS** based on confidence level and time constraints

## Future Improvements

Potential enhancements for full automation:

1. **Serial port monitoring:** Add pyserial to read Pico output directly
2. **GPIO control:** Use second Pico to press buttons programmatically
3. **Network simulation:** Use software network control instead of manual WiFi toggling
4. **Automated power cycling:** Use USB power control for Pico resets
5. **Dashboard integration:** Real-time test status display

## Questions?

See main documentation:
- `Integration-Testing.md` - Original test specifications
- `test2.txt` - Test results tracking
- `README.md` - Project overview
