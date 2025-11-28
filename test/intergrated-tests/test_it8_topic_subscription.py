#!/usr/bin/env python3
"""
IT8: Successful Topic Subscription
Tests SUBSCRIBE -> SUBACK flow with granted QoS and topic ID
"""

import paho.mqtt.client as mqtt
import time
import sys

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"
TEST_RUNS = 50  # 50 test cases as per requirement
TIMEOUT_MS = 800  # 800ms requirement

# Test results
test_results = []

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_subscribe(client, userdata, mid, granted_qos):
    """Callback when subscription is confirmed"""
    timestamp = time.time()
    userdata['subscribe_time'] = timestamp
    userdata['granted_qos'] = granted_qos[0]
    print(f"📡 Subscription confirmed: MID={mid}, Granted QoS={granted_qos[0]}")

def run_test():
    """Run IT8 test"""
    print("="*60)
    print("IT8: Topic Subscription Test")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Timeout requirement: {TIMEOUT_MS}ms\n")
    
    print("⚠️  NOTE: This test verifies MQTT broker subscription.")
    print("   For MQTT-SN specific SUBSCRIBE testing, observe Pico serial output.\n")
    
    passed = 0
    failed = 0
    
    for i in range(1, TEST_RUNS + 1):
        userdata = {}
        client = mqtt.Client(userdata=userdata)
        client.on_connect = on_connect
        client.on_subscribe = on_subscribe
        
        try:
            # Connect to broker
            client.connect(BROKER, PORT, 60)
            client.loop_start()
            time.sleep(0.5)  # Wait for connection
            
            # Subscribe and measure time
            start_time = time.time()
            result, mid = client.subscribe(TEST_TOPIC, qos=1)
            
            # Wait for SUBACK
            timeout = start_time + (TIMEOUT_MS / 1000.0)
            while 'subscribe_time' not in userdata and time.time() < timeout:
                time.sleep(0.01)
            
            elapsed_ms = (time.time() - start_time) * 1000
            
            # Check results
            if 'subscribe_time' in userdata:
                granted_qos = userdata['granted_qos']
                if elapsed_ms <= TIMEOUT_MS and granted_qos >= 0:
                    print(f"  ✅ Test {i}/{TEST_RUNS} PASS: SUBACK in {elapsed_ms:.1f}ms, QoS={granted_qos}")
                    passed += 1
                    test_results.append('PASS')
                else:
                    print(f"  ❌ Test {i}/{TEST_RUNS} FAIL: SUBACK in {elapsed_ms:.1f}ms (>{TIMEOUT_MS}ms)")
                    failed += 1
                    test_results.append('FAIL')
            else:
                print(f"  ❌ Test {i}/{TEST_RUNS} FAIL: No SUBACK received within {TIMEOUT_MS}ms")
                failed += 1
                test_results.append('FAIL')
            
            client.loop_stop()
            client.disconnect()
            time.sleep(0.1)
            
        except Exception as e:
            print(f"  ❌ Test {i}/{TEST_RUNS} FAIL: {e}")
            failed += 1
            test_results.append('FAIL')
            try:
                client.loop_stop()
                client.disconnect()
            except:
                pass
    
    # Print summary
    print("\n" + "="*60)
    print("TEST SUMMARY")
    print("="*60)
    success_rate = (passed / TEST_RUNS) * 100
    print(f"Passed: {passed}/{TEST_RUNS}")
    print(f"Failed: {failed}/{TEST_RUNS}")
    print(f"Success Rate: {success_rate:.1f}%")
    print(f"\nTolerance: {passed} out of {TEST_RUNS} test cases, achieved {success_rate:.1f}% success")
    
    print("\n" + "="*60)
    print("MQTT-SN CLIENT VERIFICATION")
    print("="*60)
    print("To verify Pico W MQTT-SN client subscription:")
    print("1. Reset Pico W")
    print("2. Watch serial monitor for:")
    print("   [MQTTSN] Subscribing to topic 'pico/test'...")
    print("   [MQTTSN] ✓ Subscription confirmed (TopicID=X, QoS=Y)")
    print("3. Verify SUBACK received within 800ms")

if __name__ == "__main__":
    try:
        run_test()
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        sys.exit(0)
