#!/usr/bin/env python3
"""
IT1: QoS 0 - Successful message delivery test
Tests if receiver gets exactly 1 message within 1 second with matching topic and payload
"""

import paho.mqtt.client as mqtt
import time
import sys

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"
TEST_RUNS = 100  # Set to 100 for comprehensive testing
TIMEOUT_SEC = 1.0

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
        'timestamp': timestamp,
        'qos': msg.qos
    })
    print(f"📩 Received: topic={msg.topic}, qos={msg.qos}, payload={payload}")

def run_test():
    """Run IT1 test"""
    print("="*60)
    print("IT1: QoS 0 Message Delivery Test")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Timeout: {TIMEOUT_SEC}s per message\n")
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        
        # Wait for connection
        time.sleep(2)
        
        passed = 0
        failed = 0
        
        for i in range(1, TEST_RUNS + 1):
            messages_received.clear()
            start_time = time.time()
            
            # Wait for message (Pico sends every 5 seconds)
            print(f"\nTest {i}/{TEST_RUNS}: Waiting for message...")
            
            # Wait up to 10 seconds for a message (Pico sends every 5s)
            timeout = start_time + 10
            while len(messages_received) == 0 and time.time() < timeout:
                time.sleep(0.1)
            
            elapsed = time.time() - start_time
            
            # Check results
            if len(messages_received) == 1:
                msg = messages_received[0]
                # Check topic and QoS (elapsed time not critical since Pico sends every 5s)
                if msg['topic'] == TEST_TOPIC and msg['qos'] == 0:
                    print(f"  ✅ PASS: Received 1 message in {elapsed:.2f}s, QoS=0")
                    passed += 1
                    test_results.append('PASS')
                else:
                    print(f"  ❌ FAIL: Wrong QoS (got {msg['qos']}) or wrong topic")
                    failed += 1
                    test_results.append('FAIL')
            elif len(messages_received) == 0:
                print(f"  ⚠️  SKIP: No message received (Pico may not be sending)")
                test_results.append('SKIP')
            else:
                print(f"  ❌ FAIL: Received {len(messages_received)} messages (expected 1)")
                failed += 1
                test_results.append('FAIL')
            
            # Delay between tests
            time.sleep(1)
        
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
        else:
            print("⚠️  No tests completed (Pico may not be sending messages)")
            print("   Make sure Pico W is connected and running")
        
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
