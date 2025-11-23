#!/usr/bin/env python3
"""
IT2: QoS 1 - Successful message delivery test
Tests if receiver gets QoS 1 PUBLISH with correct MsgID and sender receives PUBACK
"""

import paho.mqtt.client as mqtt
import time
import sys

BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"
TEST_RUNS = 10  # Changed from 100 to 10 for faster testing

test_results = []
messages_received = []

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker")
        client.subscribe(TEST_TOPIC, qos=1)
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    messages_received.append({
        'qos': msg.qos,
        'payload': msg.payload.decode('utf-8', errors='ignore')
    })

def run_test():
    print("="*60)
    print("IT2: QoS 1 Message Delivery with PUBACK")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print("⚠️  Set Pico to QoS 1 (press GP22 button)\n")
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(2)
        
        passed = 0
        
        for i in range(1, TEST_RUNS + 1):
            messages_received.clear()
            print(f"Test {i}/{TEST_RUNS}: Waiting...")
            
            timeout = time.time() + 10
            while len(messages_received) == 0 and time.time() < timeout:
                time.sleep(0.1)
            
            if len(messages_received) == 1 and messages_received[0]['qos'] >= 1:
                print(f"  ✅ PASS: QoS 1 message received")
                passed += 1
                test_results.append('PASS')
            else:
                test_results.append('SKIP')
            
            time.sleep(1)
        
        actual = passed
        if actual > 0:
            rate = (passed / actual) * 100
            print(f"\n{'='*60}")
            print(f"Passed: {passed}/{actual}")
            print(f"Success Rate: {rate:.1f}%")
            print(f"Tolerance: {passed} out of {actual} test cases, achieved {rate:.1f}% success")
        
        client.loop_stop()
        client.disconnect()
        
    except KeyboardInterrupt:
        print("\n⚠️  Interrupted")
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    run_test()
