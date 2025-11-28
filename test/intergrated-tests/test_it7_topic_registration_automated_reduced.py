#!/usr/bin/env python3
"""
IT7: Successful Topic Registration (REDUCED TEST RUNS)
Tests REGISTER -> REGACK flow by monitoring MQTT broker for registered topics

This version runs only 10 iterations instead of 50 for faster testing.
"""

import paho.mqtt.client as mqtt
import time
import sys

# Test configuration
BROKER = "localhost"
PORT = 1883
EXPECTED_TOPICS = ["pico/test", "pico/chunks"]
TEST_RUNS = 10  # Reduced from 50 to 10 for practical testing
REGISTRATION_TIMEOUT = 5  # 800ms per topic expected

# Test results
test_results = []
registered_topics = set()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        # Subscribe to $SYS topics to monitor registrations (if available)
        client.subscribe("#")  # Subscribe to all topics to detect registration
        print(f"📡 Monitoring all topics")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received - indicates topic is active"""
    topic = msg.topic
    if topic in EXPECTED_TOPICS and topic not in userdata['seen_topics']:
        userdata['seen_topics'].add(topic)
        userdata['last_topic_time'] = time.time()
        print(f"✅ Topic active: {topic}")

def restart_pico_prompt():
    """Prompt user to restart Pico"""
    print("\n" + "="*60)
    print("ACTION REQUIRED: Reset Pico W")
    print("="*60)
    print("1. Unplug and replug Pico W USB cable")
    print("   (OR press reset button if available)")
    print("2. Wait for boot sequence to complete")
    print("3. Watch for 'MQTT-SN client initialized' message")
    print()
    input("Press Enter when Pico has restarted...")

def run_test():
    """Run IT7 test"""
    print("="*60)
    print("IT7: Topic Registration (REDUCED - 10 RUNS)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Expected topics: {EXPECTED_TOPICS}")
    print(f"Registration timeout: {REGISTRATION_TIMEOUT}s\n")
    
    print("⚠️  NOTE:")
    print("  This version runs only 10 iterations (not 50) for practical testing.")
    print("  For full 50-run test, use: test_it7_topic_registration_automated.py")
    print()
    print("⚠️  ALTERNATIVE:")
    print("  Consider using IT10 (Topic Registration After Disconnect) which")
    print("  tests the same functionality but with automated gateway control")
    print("  instead of manual Pico resets.")
    print()
    
    print("⚠️  TEST METHOD:")
    print("  This test verifies topic registration by monitoring MQTT broker")
    print("  for messages on registered topics. Topics must be registered")
    print("  before Pico can publish to them.\n")
    
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
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Prompt for Pico restart
            restart_pico_prompt()
            
            # Monitor for topic activity
            userdata = {'seen_topics': set(), 'last_topic_time': None}
            client._userdata = userdata
            
            start_time = time.time()
            timeout = start_time + 30  # Give 30s for Pico to boot and register
            
            print("\n⏳ Monitoring for topic registrations...")
            
            # Wait for both topics to appear or timeout
            while len(userdata['seen_topics']) < len(EXPECTED_TOPICS) and time.time() < timeout:
                time.sleep(0.1)
            
            elapsed = time.time() - start_time
            
            # Check results
            if len(userdata['seen_topics']) == len(EXPECTED_TOPICS):
                print(f"\n✅ All topics registered in {elapsed:.2f}s")
                for topic in userdata['seen_topics']:
                    print(f"   ✓ {topic}")
                passed += 1
                test_results.append('PASS')
            else:
                missing = set(EXPECTED_TOPICS) - userdata['seen_topics']
                print(f"\n❌ Registration incomplete after {elapsed:.2f}s")
                print(f"   Missing: {missing}")
                failed += 1
                test_results.append('FAIL')
            
            if i < TEST_RUNS:
                print("\n⏳ Waiting 5s before next test...")
                time.sleep(5)
        
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
        print("SERIAL VERIFICATION")
        print("="*60)
        print("For detailed timing verification, check Pico serial output for:")
        print("  [MQTTSN] Registering topic 'pico/test'...")
        print("  [MQTTSN] ✓ Topic registered (TopicID=X, MsgID=Y)")
        print("  [MQTTSN] Registering topic 'pico/chunks'...")
        print("  [MQTTSN] ✓ Topic 'pico/chunks' registered (TopicID=Z)")
        print("\nEach registration should complete within 800ms")
        
        print("\n" + "="*60)
        print("NOTE")
        print("="*60)
        print("This test ran {TEST_RUNS} iterations. For comprehensive testing,")
        print("you can run test_it7_topic_registration_automated.py (50 runs)")
        print("or use IT10 which automates gateway disconnect/reconnect instead")
        print("of requiring manual Pico resets.")
        
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
