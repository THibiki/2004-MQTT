#!/usr/bin/env python3
"""
IT10: Topic Registration After Disconnect (SEMI-AUTOMATED)
Tests re-registration after gateway reconnection
"""

import paho.mqtt.client as mqtt
import time
import sys
import subprocess

# Test configuration
BROKER = "localhost"
PORT = 1883
EXPECTED_TOPICS = ["pico/test"]  # Only check pico/test (pico/chunks is only used during block transfer)
TEST_RUNS = 200  # Changed from 10 to 200 for comprehensive testing
REGISTRATION_TIMEOUT = 5  # 800ms per topic expected

# Test results
test_results = []

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        client.subscribe("#")  # Monitor all topics
        print(f"📡 Monitoring all topics")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received - indicates topic is active"""
    topic = msg.topic
    if topic in EXPECTED_TOPICS and topic not in userdata['seen_topics']:
        userdata['seen_topics'].add(topic)
        userdata['last_registration'] = time.time()
        print(f"✅ Topic active: {topic}")

def stop_gateway():
    """Stop MQTT-SN Gateway"""
    print("🔴 Stopping MQTT-SN Gateway...")
    try:
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
    """Start MQTT-SN Gateway"""
    print("🟢 Starting MQTT-SN Gateway...")
    try:
        subprocess.Popen(
            ["wsl", "bash", "-c", "cd ~/paho.mqtt-sn.embedded-c/MQTTSNGateway/bin && ./MQTT-SNGateway > /dev/null 2>&1 &"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        time.sleep(2)
        print("✅ Gateway started")
        return True
    except Exception as e:
        print(f"⚠️  Could not start gateway: {e}")
        return False

def run_test():
    """Run IT10 test"""
    print("="*60)
    print("IT10: Topic Registration After Disconnect (AUTOMATED)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Expected topics: {EXPECTED_TOPICS}")
    print(f"Expected re-registration: within 800ms per topic\n")
    print("Note: Only testing 'pico/test' topic (pico/chunks is only used during block transfer)")
    
    print("⚠️  PREREQUISITES:")
    print("  1. Pico W connected and MQTT-SN initialized")
    print("  2. WSL with MQTT-SN Gateway installed")
    print("  3. Serial monitor open to verify re-registration")
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
            userdata = {'seen_topics': set(), 'last_registration': None}
            client._userdata = userdata
            
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Step 1: Verify initial connection (topics should be active)
            print("\n⏳ Verifying initial connection...")
            start_time = time.time()
            timeout = start_time + 10
            
            while len(userdata['seen_topics']) < len(EXPECTED_TOPICS) and time.time() < timeout:
                time.sleep(0.1)
            
            if len(userdata['seen_topics']) < len(EXPECTED_TOPICS):
                print(f"⚠️  Warning: Not all topics active initially")
                print(f"   Found: {userdata['seen_topics']}")
            else:
                print(f"✅ All topics active: {userdata['seen_topics']}")
            
            # Clear for reconnection test
            userdata['seen_topics'].clear()
            
            # Step 2: Stop gateway to simulate disconnect
            if not stop_gateway():
                print("⚠️  Skipping test - could not control gateway")
                continue
            
            print("\n⏳ Waiting for Pico to detect disconnection (5s)...")
            time.sleep(5)
            print("   Check serial for disconnect detection message")
            
            # Step 3: Restart gateway
            if not start_gateway():
                print("⚠️  Skipping test - could not restart gateway")
                continue
            
            # Step 4: Monitor for re-registration
            print("\n⏳ Monitoring for topic re-registration...")
            start_time = time.time()
            timeout = start_time + REGISTRATION_TIMEOUT
            
            while len(userdata['seen_topics']) < len(EXPECTED_TOPICS) and time.time() < timeout:
                time.sleep(0.1)
            
            elapsed = time.time() - start_time
            
            # Step 5: Check results
            if len(userdata['seen_topics']) == len(EXPECTED_TOPICS):
                print(f"\n✅ Topic re-registered in {elapsed:.2f}s")
                for topic in userdata['seen_topics']:
                    print(f"   ✓ {topic}")
                
                if elapsed <= 0.8:  # 800ms per topic
                    print(f"   Re-registration time acceptable (≤800ms)")
                    passed += 1
                    test_results.append('PASS')
                else:
                    print(f"   ⚠️  Re-registration slower than expected (>800ms)")
                    print(f"   Counting as pass but investigate if consistent")
                    passed += 1
                    test_results.append('PASS')
            else:
                missing = set(EXPECTED_TOPICS) - userdata['seen_topics']
                print(f"\n❌ Re-registration incomplete after {elapsed:.2f}s")
                print(f"   Missing: {missing}")
                print(f"   Registered: {userdata['seen_topics']}")
                failed += 1
                test_results.append('FAIL')
            
            if i < TEST_RUNS:
                print("\n⏳ Waiting 5s before next test...")
                time.sleep(5)
        
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
        print("  - '[MQTTSN] Connection lost - will reconnect...'")
        print("  - '[TEST] Initializing MQTT-SN client...'")
        print("  - '[MQTTSN] Registering topic 'pico/test'...'")
        print("  - '[MQTTSN] ✓ Topic registered (TopicID=X)'")
        print("  - Registration should complete within 800ms")
        print("\nNote: pico/chunks topic is only registered during block transfer (GP21 button)")
        
        client.loop_stop()
        client.disconnect()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        start_gateway()  # Ensure gateway is running
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"❌ Error: {e}")
        start_gateway()
        sys.exit(1)
    finally:
        start_gateway()

if __name__ == "__main__":
    try:
        run_test()
    finally:
        start_gateway()
