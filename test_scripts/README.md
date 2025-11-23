# Integration Testing Guide

Complete guide for running all 12 integration tests (IT1-IT12) for the MQTT-SN Pico W project.

---

## Quick Reference

| Test | Type | Duration | Test Runs | Description |
|------|------|----------|-----------|-------------|
| IT1 | 🤖 Auto | 1 min | 10 | QoS 0 message delivery |
| IT2 | 🤖 Auto | 1 min | 10 | QoS 1 with PUBACK |
| IT3 | 👁️ Manual | 5 min | 1-5 | QoS 1 retry with DUP flag |
| IT4 | 👁️ Manual | 3 min | 1-5 | QoS 1 reconnect recovery |
| IT5 | 🤖 Auto | 5 min | 10 | Block transfer reassembly |
| IT6 | 👁️ Manual | 5 min | 1-5 | Fragment loss recovery |
| IT7 | 👁️ Manual | 2 min | 5-10 | Topic registration |
| IT8 | ⚠️ N/A | - | - | Not implemented |
| IT9 | 👁️ Manual | 10 min | 5-10 | WiFi auto-reconnect (✅ passed) |
| IT10 | 👁️ Manual | 5 min | 5-10 | Re-registration after disconnect |
| IT11 | 👁️ Manual | 2 min | 5-10 | WiFi connection on boot (✅ passed) |
| IT12 | 👁️ Manual | 5 min | 5-10 | SD card operations |

**Total time:** ~45 minutes for all tests

🤖 = Automated | 👁️ = Manual observation | ⚠️ = Not applicable

---

## Setup (One-Time)

### 1. Install Python Dependencies
```bash
cd C:\Users\tony9\Documents\GitHub\2004-MQTT
pip install paho-mqtt
```

### 2. Start Required Services

**WSL - Mosquitto Broker:**
```bash
sudo systemctl start mosquitto
sudo systemctl status mosquitto  # Should show "active (running)"
```

**WSL - MQTT-SN Gateway:**
```bash
cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
./MQTT-SNGateway
```

**Pico W:**
- Plug in via USB
- Open serial monitor
- Verify: `✓✓✓ SUCCESS: MQTT-SN client fully initialized`

### 3. Activate Python Virtual Environment
```bash
cd C:\Users\tony9\Documents\GitHub\2004-MQTT
venv\Scripts\activate  # You'll see (venv) in prompt
```

---

## Automated Tests (Run & Wait)

### IT1: QoS 0 Message Delivery

**Setup:** Set Pico to QoS 0 (press GP22 until serial shows `QoS 0`)

**Run:**
```bash
python test_scripts\IT1_qos0_message_delivery.py
```

**Duration:** ~1 minute (10 messages)

**Expected Result:**
```
Tolerance: X out of 10 test cases, achieved Z% success
```

---

### IT2: QoS 1 Message Delivery

**Setup:** Set Pico to QoS 1 (press GP22 until serial shows `QoS 1`)

**Run:**
```bash
python test_scripts\IT2_qos1_message_delivery.py
```

**Duration:** ~1 minute (10 messages)

**Expected Result:**
```
Tolerance: X out of 10 test cases, achieved Z% success
```

---

### IT5: Block Transfer

**Setup:**
- SD card with .jpg file (<240KB) inserted in Pico
- Any QoS level (recommend QoS 1 for reliability)

**Run:**
```bash
python test_scripts\IT5_block_transfer.py
```

**During test:** Press GP21 button 10 times (once per transfer)

**Duration:** ~5 minutes (10 transfers)

**Expected Result:**
```
Tolerance: 10 out of 10 test cases, achieved 100.0% success
```

---

## Manual Tests (Follow Instructions)

### IT3: QoS 1 Retry with DUP Flag

**Steps:**
1. Set Pico to QoS 1
2. Run: `python test_scripts\IT3_qos1_retry.py`
3. Stop MQTT-SN Gateway (Ctrl+C in Gateway terminal)
4. Watch Pico serial for retry attempts with DUP=1
5. Restart Gateway, verify PUBACK received

**Success:** Sender retransmits with DUP=1, stops after PUBACK

---

### IT4: QoS 1 Dropped Connection

**Steps:**
1. Set Pico to QoS 1
2. Run: `python test_scripts\IT4_qos1_dropped_connection.py`
3. Unplug Pico USB during message transmission
4. Wait 3 seconds, plug back in
5. Watch serial for reconnection and message resend

**Success:** After reconnect, message delivered exactly once

---

### IT6: Block Transfer with Fragment Loss

**Steps:**
1. Set Pico to QoS 1 or QoS 2
2. Run: `python test_scripts\IT6_block_transfer_fragment_loss.py`
3. Press GP21 to start transfer
4. During transfer, briefly stop Gateway (2-3 seconds)
5. Restart Gateway, watch for retransmission

**Success:** Missing fragments retransmitted, transfer completes

---

### IT7: Topic Registration

**Steps:**
1. Run: `python test_scripts\IT7_topic_registration.py`
2. Reset Pico (unplug/replug)
3. Watch serial monitor during startup for:
```
[MQTTSN] Registering topic 'pico/test'...
[MQTTSN] ✓ Topic registered (TopicID=X)
[MQTTSN] ✓ Topic 'pico/chunks' registered (TopicID=Y)
```

**Success:** Both topics registered within 800ms each

---

### IT8: Topic Subscription

**Result:** ⚠️ Not applicable - Pico only publishes, doesn't subscribe

---

### IT9: WiFi Auto-Reconnect

**Note:** ✅ Already passed (50/50 test cases, 100% success) per Integration-Testing.txt

**To verify:**
1. Disable WiFi router/access point
2. Watch Pico serial: `[WARNING] WiFi Connection Lost!`
3. Enable WiFi
4. Watch reconnection (should be 3-7 seconds)

---

### IT10: Topic Registration After Disconnect

**Steps:**
1. Run: `python test_scripts\IT10_topic_registration_after_disconnect.py`
2. Stop MQTT-SN Gateway
3. Watch Pico serial for disconnect detection
4. Restart Gateway
5. Watch for re-registration of both topics

**Success:** Topics re-registered within 800ms, usable immediately

---

### IT11: WiFi Connection on Boot

**Note:** ✅ Already passed (50/50 test cases, 100% success) per Integration-Testing.txt

**To verify:**
1. Reset Pico (unplug/replug)
2. Watch serial monitor boot sequence
3. Measure WiFi connection time

**Success:** Connected within 3-7 seconds with DHCP IP

---

### IT12: SD Card Operations

**Steps:**
1. Insert SD card with test image
2. Run: `python test_scripts\IT12_sd_card_operations.py`
3. Press GP21 button
4. Watch serial for:
```
[SD] SD card initalised and FAT32 mounted!
[APP] Scanning SD card for images...
✅ Image loaded from SD card: XXXXX bytes
✅ Block Transfer completed successfully
```

**Success:** File read without errors, transfer completes, image displays correctly

---

## Test Results Format

All automated tests output:
```
============================================================
TEST SUMMARY
============================================================
Passed: X/10
Failed: Y/10
Success Rate: Z%

Tolerance: X out of 10 test cases, achieved Z% success
```

**Copy the "Tolerance" line into Integration-Testing.txt for each test.**

---

## Tips & Best Practices

### Quick Testing (Default)
- Automated tests run 10 iterations (~1-5 min each)
- Manual tests: Do 5-10 observations
- Total time: ~45 minutes

### Extensive Testing (Final Report)
- Edit scripts: Change `TEST_RUNS = 100`
- Manual tests: Do 50+ observations
- Total time: ~3 hours

### Efficient Workflow
```bash
# Run all automated tests in sequence (~7 minutes)
python test_scripts\IT1_qos0_message_delivery.py > IT1_results.txt
# Press GP22 to change QoS
python test_scripts\IT2_qos1_message_delivery.py > IT2_results.txt
# Press GP21 10 times
python test_scripts\IT5_block_transfer.py > IT5_results.txt
```

### Troubleshooting

**"No module named paho"**
```bash
pip install paho-mqtt
```

**"Connection failed"**
```bash
# Check Mosquitto
sudo systemctl status mosquitto
# Check Gateway is running
```

**"No messages received"**
- Check Pico serial monitor
- Verify QoS level (press GP22)
- Ensure Gateway is running

**"Virtual environment not found"**
```bash
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

---

## Recording Results

1. **Run each test** following the steps above
2. **Copy "Tolerance" line** from test output
3. **Paste into Integration-Testing.txt** in the appropriate row
4. **Keep serial logs** and screenshots for documentation

---

## Notes

- ✅ Tests use existing code - no firmware changes needed
- ✅ Tests only observe behavior - non-invasive
- ✅ IT9 and IT11 already completed (100% success)
- ⚠️ IT8 not applicable (subscription not implemented)
- 🤖 3 automated tests, 👁️ 8 manual tests, ⚠️ 1 N/A

---

**Good luck with testing! 🚀**
