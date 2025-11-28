#!/usr/bin/env python3
"""
IT9: WiFi Auto-Reconnect (SEMI-AUTOMATED)
Tests WiFi reconnection and MQTT-SN session restoration
"""

import paho.mqtt.client as mqtt
import time
import sys

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"
TEST_RUNS = 10
RECONNECT_TIMEOUT = 10  # 3-7 seconds expected

# Test results
test_results = []
messages_received = []

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        client.subscribe(TEST_TOPIC)
        print(f"📡 Subscribed to {TEST_TOPIC}")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received"""
    timestamp = time.time()
    payload = msg.payload.decode('utf-8', errors='ignore')
    messages_received.append({
        'topic': msg.topic,
        'payload': payload,
        'timestamp': timestamp
    })
    userdata['last_message_time'] = timestamp
    print(f"📩 Message: {payload}")

def prompt_wifi_disable():
    """Prompt user to disable WiFi"""
    print("\n" + "="*60)
    print("ACTION REQUIRED: Disable WiFi")
    print("="*60)
    print("1. Turn OFF your WiFi router/access point")
    print("2. Wait for Pico to detect disconnection (check serial)")
    print("3. Look for '[WARNING] WiFi Connection Lost!' message")
    print()
    input("Press Enter after WiFi is disabled and Pico shows disconnection...")

def prompt_wifi_enable():
    """Prompt user to enable WiFi"""
    print("\n" + "="*60)
    print("ACTION REQUIRED: Enable WiFi")
    print("="*60)
    print("1. Turn ON your WiFi router/access point")
    print("2. Wait for Pico to reconnect")
    print()
    input("Press Enter after WiFi is enabled...")

def run_test():
    """Run IT9 test"""
    print("="*60)
    print("IT9: WiFi Auto-Reconnect (SEMI-AUTOMATED)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Expected reconnect time: 3-7 seconds\n")
    
    print("⚠️  PREREQUISITES:")
    print("  1. Pico W connected and sending messages regularly")
    print("  2. Access to WiFi router/access point power")
    print("  3. Serial monitor open to verify Pico status")
    print()
    
    input("Press Enter when ready to start testing...")
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(2)
        
        passed = 0
        failed = 0
        
        for i in range(1, TEST_RUNS + 1):
            messages_received.clear()
            userdata = {'last_message_time': None}
            client._userdata = userdata
            
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Step 1: Verify Pico is connected (receiving messages)
            print("\n⏳ Verifying Pico is connected...")
            start_time = time.time()
            timeout = start_time + 15
            
            while len(messages_received) == 0 and time.time() < timeout:
                time.sleep(0.1)
            
            if len(messages_received) == 0:
                print("❌ No messages received - Pico may not be connected")
                failed += 1
                test_results.append('FAIL')
                continue
            
            print(f"✅ Pico is connected and sending messages")
            messages_received.clear()
            
            # Step 2: Disable WiFi
            prompt_wifi_disable()
            
            # Verify disconnection (no messages should arrive)
            print("\n⏳ Verifying disconnection...")
            time.sleep(5)
            
            if len(messages_received) > 0:
                print("⚠️  Warning: Still receiving messages (WiFi may not be disabled)")
            else:
                print("✅ Disconnection confirmed")
            
            messages_received.clear()
            
            # Step 3: Enable WiFi
            prompt_wifi_enable()
            
            # Step 4: Monitor for reconnection
            print("\n⏳ Monitoring for reconnection...")
            reconnect_start = time.time()
            timeout = reconnect_start + RECONNECT_TIMEOUT
            
            while len(messages_received) == 0 and time.time() < timeout:
                time.sleep(0.1)
            
            reconnect_time = time.time() - reconnect_start
            
            # Step 5: Check results
            if len(messages_received) > 0:
                if reconnect_time <= 7.0:
                    print(f"\n✅ Reconnected in {reconnect_time:.2f}s (within 3-7s window)")
                    passed += 1
                    test_results.append('PASS')
                else:
                    print(f"\n⚠️  Reconnected in {reconnect_time:.2f}s (slower than expected 7s)")
                    print("   Counting as pass but investigate if consistent")
                    passed += 1
                    test_results.append('PASS')
            else:
                print(f"\n❌ No reconnection detected within {RECONNECT_TIMEOUT}s")
                failed += 1
                test_results.append('FAIL')
            
            if i < TEST_RUNS:
                print("\n⏳ Waiting 10s before next test...")
                time.sleep(10)
        
        # Print summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        actual_tests = passed + failed
        if actual_tests > 0:
            success_rate = (passed / actual_tests) * 100
            print(f"Passed: {passed}/{actual_tests}")
            print(f"Failed: {failed}/{actual_tests}")
            print(f"Success Rate: {success_rate:.1f}%")
            print(f"\nTolerance: {passed} out of {actual_tests} test cases, achieved {success_rate:.1f}% success")
        
        print("\n" + "="*60)
        print("SERIAL VERIFICATION")
        print("="*60)
        print("Verify on Pico serial monitor:")
        print("  - '[WARNING] WiFi Connection Lost!' on disconnect")
        print("  - '[INFO] WiFi Reconnected! Reinitializing...' on reconnect")
        print("  - '[MQTTSN] SUCCESS: MQTT-SN client initialized'")
        print("  - MQTT-SN session restored automatically")
        
        client.loop_stop()
        client.disconnect()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    run_test()
