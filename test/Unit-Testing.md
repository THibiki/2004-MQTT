# PICO W MQTT-SN CLIENT - UNIT TEST SUITE

Comprehensive unit tests for Pico W MQTT-SN client testing gateway UDP transmission, timeout handling, and microSD performance.

---

## Quick Start

```powershell
# 1. Stop Paho gateway (in WSL)
pkill MQTT-SNGateway

# 2. Run tests (in PowerShell)
cd c:\Programming\2004-MQTT\tests
python run_all_tests.py

# 3. Restart Paho gateway (in WSL)
cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

---

## Prerequisites

### Hardware
- **Pico W** powered on and connected to WiFi
- **microSD card** (for UT3 only)
- **USB connection** to Pico (for monitoring/debugging)

### Software
- **Python 3.7+** on Windows
- **WSL with Paho MQTT-SN Gateway** (for actual gateway)
- **Mosquitto broker** running (for MQTT backend)
- **PowerShell as Administrator** (for UT2 firewall tests)

### Pico W Configuration
Verify `network_config.h` on Pico:

```c
#define MQTTSN_GATEWAY_IP "YOUR_LAPTOP_IP"    // Laptop's IP
#define MQTTSN_GATEWAY_PORT 1884          // Gateway port
```

### System Setup Checklist

- [ ] Pico W is powered on
- [ ] Pico W is connected to WiFi
- [ ] Paho gateway is **STOPPED** before running tests
- [ ] Mosquitto broker is running
- [ ] PowerShell is running as **ADMINISTRATOR** (for UT2)
- [ ] Tests directory: `c:\Programming\2004-MQTT\test\`

---

## Test Suite Overview

### UT1: Successful UDP Transmission (with Stress Test)
**Duration:** ~20 minutes  
**Iterations:** 100 packets

**What it tests:**
- ✓ Gateway receives UDP packet from Pico
- ✓ Payload matches sent data byte-for-byte
- ✓ All 100 packets are identical (no corruption)
- ✓ Packet sizes are consistent
- ✓ Timing intervals are consistent (~10s)

**How it works:**
1. Listens on port 1884
2. Receives CONNECT packets from Pico (retries every 10s)
3. Collects 100 packets over ~20 minutes
4. Verifies all packets are identical
5. Reports consistency metrics

**Prerequisites:**
- Pico W powered on and connected to WiFi
- Paho gateway **STOPPED**
- Mosquitto broker running
- Stable power to Pico (don't unplug!)

---

### UT2: Timeout Handling
**Duration:** ~50 seconds

**What it tests:**
- ✓ Pico detects unreachable gateway (no response)
- ✓ Pico retries after timeout
- ✓ Pico recovers when gateway becomes available again

**How it works:**
1. Blocks UDP port 1884 (simulates unreachable gateway)
2. Waits 20 seconds (Pico attempts to send, fails)
3. Unblocks port
4. Listens for reconnection attempts
5. Verifies Pico successfully reconnects

**Prerequisites:**
- Pico W powered on and connected to WiFi
- Paho gateway **STOPPED**
- **PowerShell running as ADMINISTRATOR**
- Mosquitto broker running

**Why Admin?**
Windows firewall rules require administrator privileges to add/remove rules.

---

### UT3: microSD Performance
**Duration:** ~2 minutes (or custom)

**What it tests:**
- ✓ microSD read/write performance for 512B chunks
- ✓ microSD read/write performance for 1kB chunks
- ✓ 95% of operations ≤ 15ms
- ✓ 100% of operations ≤ 20ms (max latency)

**How it works:**
1. Listens on port 1884
2. Receives SD timing data from Pico
3. Collects multiple samples
4. Analyzes P95 and max latencies
5. Reports pass/fail based on requirements

**Prerequisites:**
- Pico W has microSD card inserted
- Pico W powered on and connected to WiFi
- Paho gateway **STOPPED**
- Pico firmware supports SD performance testing

---

### Run Individual Tests

#### UT1: Successful UDP Transmission

```powershell
# Default: 100 iterations, 1200s timeout (~20 minutes)
python test_ut1_udp_transmission.py 1884 1200 100

# Quick test: 50 iterations (~10 minutes)
python test_ut1_udp_transmission.py 1884 600 50

# Extended test: 200 iterations (~40 minutes)
python test_ut1_udp_transmission.py 1884 2400 200
```

**Parameters:**
- `port` (default: 1884) - Gateway listening port
- `timeout_sec` (default: 1200) - Maximum test duration in seconds
- `iterations` (default: 100) - Number of packets to collect

---

#### UT2: Timeout Handling

```powershell
# Default: block for 20s, listen for 30s (total 50s)
python test_ut2_timeout_handling.py 1884 20 30

# Extended: block for 40s, listen for 60s (total 100s)
python test_ut2_timeout_handling.py 1884 40 60
```

**Parameters:**
- `port` (default: 1884) - Gateway listening port
- `block_duration` (default: 20) - How long to block port (seconds)
- `listen_after` (default: 30) - How long to listen after unblocking (seconds)

**⚠ Important:** Requires PowerShell as **ADMINISTRATOR**

---

#### UT3: microSD Performance

```powershell
# Default: 120s timeout
python test_ut3_microsd_performance.py 1884 120

# Extended: 300s timeout (for more samples)
python test_ut3_microsd_performance.py 1884 300
```

**Parameters:**
- `port` (default: 1884) - Gateway listening port
- `timeout_sec` (default: 120) - Maximum test duration in seconds

---

## Expected Output

### UT1 Success
```
[10/100] Packets received (1.7m)
[20/100] Packets received (3.3m)
...
[100/100] Packets received (16.8m)

STRESS TEST ANALYSIS (100 iterations)
------

1. Packet Size Consistency:
   Expected size: 18 bytes
   All packets match: ✓ YES

2. Payload Consistency (byte-for-byte):
   Total packets: 100
   All identical: ✓ YES

3. Timing Consistency:
   Average interval: 10.05s (expected ~10s)
   Consistent timing: ✓ YES

✓ STRESS TEST PASSED

FINAL RESULT
------
Requirement 1: ✓ PASS
Requirement 2: ✓ PASS
Stress Test: ✓ PASS

✓ PASS
```

### UT2 Success
```
--- Phase 1: Blocking port 1884 ---
✓ Blocked UDP port 1884
Port blocked for 20s...
(Pico attempts to send, fails)

--- Phase 2: Unblocking port ---
✓ Unblocked UDP port 1884
Listening for reconnection attempts (30s)...

✓ Reconnection packet 1 received
  Time after unblock: 0.15s

RESULTS
------
✓ Requirement 1: Gateway simulated as unreachable
✓ Requirement 2: No response received during block
✓ Requirement 3: Pico retried after unblock
  Reconnection packets received: 1
  Pico successfully retried: ✓ YES

✓ PASS
```

### UT3 Success
```
[Packet 1] Received at 0.1s
  Data: SD_512R_8.5 SD_512W_7.2 SD_1024R_9.1 SD_1024W_10.3

[Packet 2] Received at 10.1s
  Data: SD_512R_8.4 SD_512W_7.1 SD_1024R_9.0 SD_1024W_10.2

ANALYSIS
------

512B_read:
  Count: 2 operations
  Avg: 8.45ms
  P95: 8.50ms (Requirement: ≤ 15ms) ✓ PASS
  Max: 8.50ms (Requirement: ≤ 20ms) ✓ PASS

512B_write:
  Count: 2 operations
  Avg: 7.15ms
  P95: 7.20ms (Requirement: ≤ 15ms) ✓ PASS
  Max: 7.20ms (Requirement: ≤ 20ms) ✓ PASS

✓ PASS
```

---

## Troubleshooting

### UT1: No packet received

**Problem:** `✗ No packet received from Pico!`

**Solutions:**
1. ✓ Is Pico powered on?
2. ✓ Is Pico connected to WiFi?
3. ✓ Check Pico serial output for errors (USB)
4. ✓ Verify `network_config.h` has correct IP/port
5. ✓ Is Paho gateway really stopped?

```powershell
# Verify port is not in use
netstat -an | findstr 1884

# Kill any process on port 1884
netstat -ano | findstr :1884
taskkill /PID <PID> /F
```

---

### UT2: Cannot block port (requires Admin)

**Problem:** `⚠ Could not block port (requires Admin)`

**Solution:**
- Right-click PowerShell
- Select "Run as administrator"
- Retry test

---

### UT2: Firewall rule already exists

**Problem:** Rule already added from previous test

**Solution:**
```powershell
# List firewall rules
netsh advfirewall firewall show rule name="Block_MQTT_SN_1884"

# Delete rule manually
netsh advfirewall firewall delete rule name="Block_MQTT_SN_1884"
```

---

### UT3: No SD timing data received

**Problem:** `✗ No packets received from Pico!`

**Solutions:**
1. ✓ Is microSD card inserted?
2. ✓ Does Pico firmware support SD performance test?
3. ✓ Check Pico serial output for SD errors
4. ✓ Is Pico powered on and connected to WiFi?

---

## Before Starting Tests

### Checklist

```bash
# 1. Stop Paho gateway (in WSL)
pkill MQTT-SNGateway

# 2. Verify Mosquitto is running
mosquitto_sub -t 'test' -h localhost

# 3. Verify Pico is connected to WiFi
# (Check serial output or network)

# 4. Verify port 1884 is free
netstat -an | findstr 1884

# 5. Launch PowerShell as Administrator
# (for UT2)

# 6. Start tests
cd c:\Programming\2004-MQTT\tests
python run_all_tests.py
```

---

## After Running Tests

### Restart Gateway

```bash
# In WSL
cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

### Verify Pico Reconnects

- Monitor serial output of Pico
- Should see CONNACK from gateway
- Data should flow normally

---

## Test Timing

| Test | Duration | Timeout |
|------|----------|---------|
| UT1 (100 iter) | ~17 min | 20 min |
| UT1 (50 iter) | ~8 min | 10 min |
| UT2 | ~50 sec | ~1 min |
| UT3 (default) | ~2 min | 2 min |

---

## Notes

- **Pico retries every 10s** if no CONNACK received (this is normal)
- **Do not interrupt tests** - especially UT1 (takes 20 minutes)
- **Pico needs stable power** - don't unplug during tests
- **UT2 requires Admin** - right-click PowerShell, "Run as administrator"
- **Tests use port 1884** - the real gateway port (not mocked)

---

## File Structure

```
c:\Programming\2004-MQTT\
├── tests\
│   ├── unit-tests\
│   │   ├── test_config.py                    # Shared utilities
│   │   ├── test_ut1_udp_transmission.py      # UT1 test
│   │   ├── test_ut2_timeout_handling.py      # UT2 test
│   │   ├── test_ut3_microsd_performance.py   # UT3 test
│   ├── Unit-Testing.md                              # This file
└── ...
```

---

## Questions?

Check the following:

1. **Is Pico connected to WiFi?** → Check serial output
2. **Is gateway stopped?** → `pkill MQTT-SNGateway`
3. **Is port 1884 free?** → `netstat -an | findstr 1884`
4. **Is Mosquitto running?** → `mosquitto_sub -t 'test'`
5. **Is PowerShell Admin?** → Required for UT2

---

## Summary

| Test | Purpose | Duration | Admin? |
|------|---------|----------|--------|
| UT1 | UDP transmission + stress (100x) | 20 min | No |
| UT2 | Timeout handling | 1 min | **Yes** |
| UT3 | microSD performance | 2 min | No |

**Start with:** `python run_all_tests.py`

**Expected result:** All tests ✓ PASS