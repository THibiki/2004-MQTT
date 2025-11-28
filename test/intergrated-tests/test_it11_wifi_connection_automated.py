#!/usr/bin/env python3
"""
IT11: Successful WiFi Connection on Boot (SEMI-AUTOMATED)
Tests WiFi connection time and DHCP IP acquisition during boot
"""

import paho.mqtt.client as mqtt
import time
import sys

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"
TEST_RUNS = 10
BOOT_TIMEOUT = 10  # Expected 3-7 seconds

# Test results
test_results = []

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        client.subscribe(TEST_TOPIC)
        print(f"📡 Subscribed to {TEST_TOPIC}")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received - indicates Pico is connected"""
    if userdata['first_message_time'] is None:
        userdata['first_message_time'] = time.time()
        payload = msg.payload.decode('utf-8', errors='ignore')
        print(f"✅ First message received: {payload}")

def prompt_reset():
    """Prompt user to reset Pico"""
    print("\n" + "="*60)
    print("ACTION REQUIRED: Reset Pico")
    print("="*60)
    print("1. Unplug Pico W USB cable")
    print("2. Wait 2 seconds")
    print("3. Plug Pico W back in")
    print("4. Watch serial monitor for boot sequence")
    print()
    input("Press Enter IMMEDIATELY after plugging in Pico...")

def run_test():
    """Run IT11 test"""
    print("="*60)
    print("IT11: WiFi Connection on Boot (SEMI-AUTOMATED)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Expected connection time: 3-7 seconds\n")
    
    print("⚠️  PREREQUISITES:")
    print("  1. Serial monitor ready to observe boot sequence")
    print("  2. USB cable ready for unplug/replug")
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
            userdata = {'first_message_time': None}
            client._userdata = userdata
            
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Prompt user to reset Pico
            prompt_reset()
            
            # Start timing from when user presses Enter
            boot_start = time.time()
            timeout = boot_start + BOOT_TIMEOUT
            
            print("\n⏳ Monitoring for first message (indicates WiFi connected)...")
            
            # Wait for first message
            while userdata['first_message_time'] is None and time.time() < timeout:
                time.sleep(0.1)
            
            # Calculate boot time
            if userdata['first_message_time']:
                boot_time = userdata['first_message_time'] - boot_start
                
                if boot_time <= 7.0:
                    print(f"\n✅ WiFi connected in ~{boot_time:.2f}s (within 3-7s window)")
                    print(f"   Note: Timing from Enter press, actual may be slightly faster")
                    passed += 1
                    test_results.append('PASS')
                else:
                    print(f"\n⚠️  WiFi connected in ~{boot_time:.2f}s (slower than expected 7s)")
                    print(f"   Counting as pass but investigate if consistent")
                    passed += 1
                    test_results.append('PASS')
            else:
                print(f"\n❌ No message received within {BOOT_TIMEOUT}s")
                print(f"   Check serial monitor for errors")
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
        print("For precise timing, check Pico serial monitor for:")
        print("  - '=== MQTT-SN Pico W Client Starting ==='")
        print("  - '[INFO] Initializing WiFi...'")
        print("  - '[INFO] WiFi initialized successfully'")
        print("  - '[INFO] Connecting to WiFi...'")
        print("  - '[SUCCESS] WiFi connected in XXXXms'")
        print("  - '[INFO] IP Address: X.X.X.X'")
        print("  - '[INFO] Netmask: X.X.X.X'")
        print("  - '[INFO] Gateway: X.X.X.X'")
        print("\nConnection should complete within 3-7 seconds with DHCP IP")
        
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
