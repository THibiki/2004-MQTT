# Integration Test Execution Checklist

Use this checklist when running the automated integration test suite.

## Pre-Test Setup

### ☐ Environment Setup
- [ ] Python 3.x installed and in PATH
- [ ] `pip install paho-mqtt` completed successfully
- [ ] WSL installed and configured
- [ ] Test directory: `C:\Users\tony9\Documents\GitHub\2004-MQTT\test\intergrated-tests`

### ☐ Services Running
- [ ] **Mosquitto Broker** running in WSL
  ```bash
  sudo systemctl start mosquitto
  sudo systemctl status mosquitto  # Should show "active"
  ```
- [ ] **MQTT-SN Gateway** running in WSL
  ```bash
  cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin
  ./MQTT-SNGateway  # Should show gateway startup messages
  ```

### ☐ Hardware Setup
- [ ] Pico W connected via USB
- [ ] Serial monitor open (e.g., PuTTY, Tera Term, or VSCode Serial Monitor)
- [ ] Serial output shows: `✓✓✓ SUCCESS: MQTT-SN client fully initialized`
- [ ] For IT12: SD card with image file inserted in Pico

### ☐ Pico Configuration
- [ ] QoS level set correctly (press GP22 to toggle)
  - IT1: QoS 0
  - IT2, IT3, IT4: QoS 1
  - IT6: QoS 1 or QoS 2
  - Others: Any QoS
- [ ] Pico is publishing messages regularly (check serial output)

## Test Execution

### ☐ Choose Test Method

**Option A: Run All Tests (~2 hours)**
```bash
python run_automated_tests.py all
```

**Option B: Run Automated Only (~3 minutes)**
```bash
python run_automated_tests.py auto
```

**Option C: Run Individual Tests**
```bash
python test_it3_qos1_retry_automated.py
# ... repeat for each test
```

### ☐ Fully Automated Tests (No Intervention Required)

#### IT1: QoS 0 Message Delivery
- [ ] Test started: `python test_it1_qos0_message_delivery.py`
- [ ] Set Pico to QoS 0 (press GP22)
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT2: QoS 1 Message Delivery
- [ ] Test started: `python test_it2_qos1_message_delivery.py`
- [ ] Set Pico to QoS 1 (press GP22)
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT8: Topic Subscription
- [ ] Test started: `python test_it8_topic_subscription.py`
- [ ] Test completed successfully (50 subscriptions)
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

### ☐ Semi-Automated Tests (Manual Intervention Required)

#### IT3: QoS 1 Retry with DUP Flag
- [ ] Test started: `python test_it3_qos1_retry_automated.py`
- [ ] Set Pico to QoS 1
- [ ] Gateway automatically stopped/started by test
- [ ] Monitored serial output for DUP=1 flag
- [ ] Verified retransmission behavior
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT4: QoS 1 Dropped Connection
- [ ] Test started: `python test_it4_qos1_dropped_connection_automated.py`
- [ ] Set Pico to QoS 1
- [ ] Followed prompts to unplug/replug Pico (10 times)
- [ ] Verified reconnection and message delivery
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT6: Block Transfer Fragment Loss
- [ ] Test started: `python test_it6_block_transfer_fragment_loss_automated.py`
- [ ] SD card with image inserted
- [ ] Followed prompts to press GP21 (10 times)
- [ ] Gateway automatically stopped/started by test
- [ ] Verified fragment retransmission in serial output
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT7: Topic Registration
- [ ] Test started: `python test_it7_topic_registration_automated.py`
- [ ] Followed prompts to reset Pico (10 times)
- [ ] Verified registration messages in serial output
- [ ] Checked timing < 800ms per topic
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT9: WiFi Auto-Reconnect
- [ ] Test started: `python test_it9_wifi_auto_reconnect_automated.py`
- [ ] Followed prompts to disable WiFi router (10 times)
- [ ] Waited for disconnect message in serial
- [ ] Followed prompts to enable WiFi router
- [ ] Verified reconnection within 3-7 seconds
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT10: Topic Registration After Disconnect
- [ ] Test started: `python test_it10_topic_registration_after_disconnect_automated.py`
- [ ] Gateway automatically stopped/started by test
- [ ] Monitored serial for re-registration messages
- [ ] Verified topics re-registered within timeout
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT11: WiFi Connection on Boot
- [ ] Test started: `python test_it11_wifi_connection_automated.py`
- [ ] Followed prompts to reset Pico (10 times)
- [ ] Verified boot sequence in serial output
- [ ] Checked WiFi connection within 3-7 seconds
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

#### IT12: SD Card Operations
- [ ] Test started: `python test_it12_sd_card_operations_automated.py`
- [ ] SD card with image file inserted
- [ ] Followed prompts to press GP21 (10 times)
- [ ] Verified SD card initialization in serial
- [ ] Verified successful file read and transfer
- [ ] Test completed successfully
- [ ] Copy tolerance line to test2.txt
- [ ] Success rate recorded: _____%

## Post-Test Verification

### ☐ Review Test Results
- [ ] All tests completed without crashes
- [ ] Success rates documented for each test
- [ ] Any failures investigated and documented
- [ ] Serial output reviewed for anomalies

### ☐ Documentation Updates
- [ ] test2.txt updated with tolerance lines from all tests
- [ ] Any issues or observations documented
- [ ] Test completion date recorded: __________

### ☐ Services Cleanup
- [ ] MQTT-SN Gateway still running (Ctrl+C to stop if needed)
- [ ] Mosquitto broker still running (can leave running)
- [ ] Serial monitor closed or ready for next use

## Test Results Summary

Fill in after completing all tests:

```
Test  | Pass/Fail | Success Rate | Notes
------|-----------|--------------|-------
IT1   |           |          %   |
IT2   |           |          %   |
IT3   |           |          %   |
IT4   |           |          %   |
IT6   |           |          %   |
IT7   |           |          %   |
IT8   |           |          %   |
IT9   |           |          %   |
IT10  |           |          %   |
IT11  |           |          %   |
IT12  |           |          %   |
------|-----------|--------------|-------
Total | X/11 Pass | Average: _% |
```

## Troubleshooting During Tests

### If Test Hangs
1. Check Pico is still responsive (serial output)
2. Check services are still running (broker, gateway)
3. Ctrl+C to interrupt test
4. Restart Pico and/or services
5. Resume testing

### If Success Rate is Low (<90%)
1. Review serial output for errors
2. Check network stability
3. Verify Pico firmware is latest version
4. Increase timeouts in test scripts if timing issues
5. Re-run failed tests individually

### If Gateway Control Fails (IT3, IT6, IT10)
1. Verify WSL is running: `wsl --status`
2. Check gateway path in test scripts
3. Manually verify: `wsl pkill -f MQTT-SNGateway`
4. Manually restart: `wsl bash -c "cd ~/path && ./MQTT-SNGateway &"`
5. Update path in test scripts if needed

## Time Tracking

Start Time: __________
End Time: __________
Total Duration: __________ hours

## Notes and Observations

```
(Use this space for any observations, issues, or notes during testing)




```

## Sign-Off

- [ ] All tests completed
- [ ] Results documented
- [ ] Issues noted and addressed
- [ ] Ready for final report

Tested by: __________
Date: __________
Signature: __________
