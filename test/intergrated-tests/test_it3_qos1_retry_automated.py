#!/usr/bin/env python3
"""
IT3: QoS 1 - Sender resends message after timeout (AUTOMATED)
Tests that sender retransmits PUBLISH with DUP=1 after no PUBACK received
"""

import paho.mqtt.client as mqtt
import time
import sys
import subprocess
import signal
import os

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"
TEST_RUNS = 100  # Increased to 100 for comprehensive testing
TIMEOUT_SEC = 5.0  # Pico timeout for retransmission

# Test results
test_results = []
messages_received = []
gateway_process = None

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

def stop_gateway():
    """Stop MQTT-SN Gateway using WSL"""
    print("\n🔴 Stopping MQTT-SN Gateway...")
    try:
        # Kill the gateway process via WSL
        subprocess.run(
            ["wsl", "pkill", "-f", "MQTT-SNGateway"],
            capture_output=True,
            timeout=5
        )
        time.sleep(1)
        print("✅ Gateway stopped")
        return True
    except Exception as e:
        print(f"⚠️  Could not stop gateway: {e}")
        return False

def start_gateway():
    """Start MQTT-SN Gateway using WSL"""
    print("\n🟢 Starting MQTT-SN Gateway...")
    try:
        # Start gateway in background via WSL
        subprocess.Popen(
            ["wsl", "bash", "-c", "cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin && ./MQTT-SNGateway > /dev/null 2>&1 &"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        time.sleep(2)  # Give gateway time to start
        print("✅ Gateway started")
        return True
    except Exception as e:
        print(f"⚠️  Could not start gateway: {e}")
        return False

def run_test():
    """Run IT3 test"""
    print("="*60)
    print("IT3: QoS 1 Retry with DUP Flag (AUTOMATED)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Expected retry timeout: ~{TIMEOUT_SEC}s\n")
    
    print("⚠️  PREREQUISITES:")
    print("  1. Pico W must be set to QoS 1 (press GP22)")
    print("  2. Pico W must be connected and running")
    print("  3. WSL with MQTT-SN Gateway installed")
    print()
    
    input("Press Enter when ready to start testing...")
    
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
            
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Step 1: Stop gateway to prevent PUBACK
            if not stop_gateway():
                print("⚠️  Skipping test - could not control gateway")
                continue
            
            # Step 2: Wait for Pico to send initial message (should timeout)
            print("\n⏳ Waiting for Pico to send message and timeout (~5-10s)...")
            time.sleep(10)  # Wait for initial send + timeout
            
            # Step 3: Restart gateway
            if not start_gateway():
                print("⚠️  Skipping test - could not restart gateway")
                continue
            
            # Step 4: Wait for retransmission with DUP=1
            print("\n⏳ Waiting for retransmission (should have DUP=1)...")
            start_time = time.time()
            timeout = start_time + 15  # Wait up to 15s for retry
            
            initial_count = len(messages_received)
            while len(messages_received) == initial_count and time.time() < timeout:
                time.sleep(0.1)
            
            elapsed = time.time() - start_time
            
            # Check results
            if len(messages_received) > initial_count:
                print(f"✅ Received retransmission after {elapsed:.2f}s")
                print(f"   Note: DUP flag verification requires serial monitoring")
                passed += 1
                test_results.append('PASS')
            else:
                print(f"❌ No retransmission received within 15s")
                failed += 1
                test_results.append('FAIL')
            
            # Clear messages for next test
            messages_received.clear()
            time.sleep(5)  # Delay between tests
        
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
            print("⚠️  No tests completed")
        
        print("\n" + "="*60)
        print("ADDITIONAL VERIFICATION")
        print("="*60)
        print("To verify DUP=1 flag, check Pico serial output for:")
        print("  - Initial PUBLISH with DUP=0")
        print("  - Retransmitted PUBLISH with DUP=1")
        print("  - Retries stop after PUBACK received")
        
        client.loop_stop()
        client.disconnect()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        start_gateway()  # Ensure gateway is running
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"❌ Error: {e}")
        start_gateway()  # Ensure gateway is running
        sys.exit(1)

if __name__ == "__main__":
    try:
        run_test()
    finally:
        # Ensure gateway is restarted
        start_gateway()
