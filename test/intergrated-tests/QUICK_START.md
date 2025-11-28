# Quick Start Guide - Automated Integration Tests

## TL;DR - Run All Tests

```bash
cd C:\Users\tony9\Documents\GitHub\2004-MQTT\test\intergrated-tests
python run_automated_tests.py all
```

## Prerequisites (5 Minutes Setup)

### 1. Install Python Dependency
```bash
pip install paho-mqtt
```

### 2. Start WSL Services

**Terminal 1 - Mosquitto Broker:**
```bash
sudo systemctl start mosquitto
```

**Terminal 2 - MQTT-SN Gateway:**
```bash
cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

### 3. Connect Pico W
- Plug in via USB
- Open serial monitor
- Verify: `✓✓✓ SUCCESS: MQTT-SN client fully initialized`

## Run Tests

### Option 1: Run All Tests (~2 hours)
```bash
python run_automated_tests.py all
```

### Option 2: Quick Test (Fully Automated Only, ~3 minutes)
```bash
python run_automated_tests.py auto
```
Runs IT1, IT2, IT8 with no manual intervention.

### Option 3: Individual Test
```bash
# Example: Run IT3
python test_it3_qos1_retry_automated.py
```

## What to Expect

### Fully Automated Tests (IT1, IT2, IT8)
- No manual intervention required
- Just let them run
- Watch the output

### Semi-Automated Tests (IT3, IT4, IT6, IT7, IT9, IT10, IT11, IT12)
- Script will prompt you when action is needed
- Examples:
  - "Press Enter after unplugging Pico..."
  - "Press Enter after pressing GP21..."
  - "Press Enter after disabling WiFi..."
- Follow the on-screen instructions

## Test Results

Each test outputs:
```
Tolerance: X out of Y test cases, achieved Z% success
```

**Copy this line to test2.txt for your records.**

## Common Issues

### "No messages received"
- Check Pico serial monitor shows it's sending
- Press GP22 to verify QoS level
- Restart Pico if needed

### "Connection failed"
- Verify Mosquitto: `sudo systemctl status mosquitto`
- Verify Gateway is running (check Terminal 2)

### "Could not control gateway"
- Gateway control tests (IT3, IT6, IT10) use WSL commands
- Verify gateway path in scripts matches your installation

## Adjusting Test Runs

Want faster testing? Edit the test file:

```python
# Change this line
TEST_RUNS = 10  # Default

# To this for faster tests
TEST_RUNS = 5

# Or this for comprehensive testing
TEST_RUNS = 50
```

## Test Summary

| Test | Time | Manual Actions | Prerequisites |
|------|------|----------------|---------------|
| IT1  | 1 min | None | QoS 0 |
| IT2  | 1 min | None | QoS 1 |
| IT3  | 20 min | Monitor serial | QoS 1 |
| IT4  | 20 min | Unplug/replug Pico | QoS 1 |
| IT6  | 20 min | Press GP21 | SD card inserted |
| IT7  | 50 min* | Reset Pico | - |
| IT8  | 30 sec | None | - |
| IT9  | 20 min | WiFi on/off | - |
| IT10 | 10 min | Monitor serial | - |
| IT11 | 5 min | Reset Pico | - |
| IT12 | 10 min | Press GP21 | SD card with image |

*Reduce IT7 TEST_RUNS from 50 to 10 for faster testing (~10 minutes)

## Need Help?

See detailed documentation:
- `README_AUTOMATED_TESTS.md` - Comprehensive guide
- `Integration-Testing.md` - Original test specifications
- Serial monitor output - Real-time Pico status

## After Testing

1. Review serial monitor output for any errors
2. Copy "Tolerance" lines from each test to test2.txt
3. Check success rates meet requirements (typically >90%)
4. Investigate any failed tests using serial logs
