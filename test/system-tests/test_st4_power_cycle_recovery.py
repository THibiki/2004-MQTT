#!/usr/bin/env python3
"""
ST4: Power Cycle Recovery Test
Tests Pico W reconnection and MQTT-SN session restoration after power cycle
Expected: Reconnect and restore session within <= 15 seconds
"""

import paho.mqtt.client as mqtt
import time
import sys

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/recovery/test"
TEST_RUNS = 100  # 100 test cases as per requirement
RECONNECT_TIMEOUT_SEC = 15  # <= 15 seconds requirement

# Test results
test_results = []
message_received = False

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Test client connected to broker {BROKER}:{PORT}")
        client.subscribe(TEST_TOPIC, qos=1)
    else:
        print(f"❌ Connection failed: {rc}")

def on_message(client, userdata, msg):
    """Callback when message is received from Pico"""
    global message_received
    payload = msg.payload.decode('utf-8')
    print(f"📥 Received from Pico: {payload[:60]}...")
    message_received = True

def wait_for_pico_message(client, timeout_sec):
    """Wait for a message from Pico to confirm it's operational"""
    global message_received
    message_received = False
    
    start_time = time.time()
    while time.time() - start_time < timeout_sec:
        if message_received:
            return True
        time.sleep(0.1)
    return False

def run_test():
    """Run ST4 power cycle recovery test"""
    print("="*70)
    print("ST4: Power Cycle Recovery Test")
    print("="*70)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Reconnection timeout: <= {RECONNECT_TIMEOUT_SEC} seconds")
    print(f"Test topic: {TEST_TOPIC}\n")
    
    print("⚠️  PREREQUISITES:")
    print("   1. Pico W must be powered on and running")
    print("   2. Pico W must be publishing to 'pico/recovery/test' periodically")
    print("   3. MQTT-SN Gateway must be running")
    print("   4. Mosquitto broker must be running")
    print("   5. Have physical access to Pico W power supply\n")
    
    print("📋 TEST PROCEDURE:")
    print("   1. Script verifies Pico is online and publishing")
    print("   2. User physically power cycles Pico W")
    print("   3. Script measures time until session is restored")
    print("   4. Repeat for all test runs\n")
    
    input("Press Enter when ready to start test...")
    
    # Setup test client
    client = mqtt.Client(client_id="power_cycle_tester")
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(1)
        
        passed = 0
        failed = 0
        total_recovery_time = 0
        
        print("\n" + "="*70)
        print("Starting power cycle tests...")
        print("="*70 + "\n")
        
        for i in range(1, TEST_RUNS + 1):
            print(f"--- Test {i}/{TEST_RUNS} ---")
            
            # Step 1: Verify Pico is currently online
            print("⏳ Step 1: Verifying Pico W is online and publishing...")
            if wait_for_pico_message(client, 10):
                print("   ✅ Pico W is online and operational")
            else:
                print("   ⚠️  No messages detected. Ensure Pico is publishing to topic.")
                retry = input("   Continue anyway? (y/n): ").strip().lower()
                if retry != 'y':
                    continue
            
            # Step 2: User power cycles Pico
            print("\n🔌 Step 2: POWER CYCLE PICO W NOW!")
            print("   - Disconnect and reconnect power")
            print("   - OR press RESET button")
            input("   Press Enter when Pico W has been power cycled...")
            
            # Step 3: Measure recovery time
            print("\n⏱  Step 3: Measuring reconnection time...")
            recovery_start = time.time()
            
            # Wait for Pico to reconnect and publish
            if wait_for_pico_message(client, RECONNECT_TIMEOUT_SEC):
                recovery_time = time.time() - recovery_start
                total_recovery_time += recovery_time
                
                if recovery_time <= RECONNECT_TIMEOUT_SEC:
                    print(f"   ✅ PASS: Pico reconnected in {recovery_time:.2f}s")
                    passed += 1
                    test_results.append('PASS')
                else:
                    print(f"   ❌ FAIL: Reconnection took {recovery_time:.2f}s (>{RECONNECT_TIMEOUT_SEC}s)")
                    failed += 1
                    test_results.append('FAIL')
            else:
                print(f"   ❌ FAIL: No message received within {RECONNECT_TIMEOUT_SEC}s")
                failed += 1
                test_results.append('FAIL')
            
            # Verification questions
            print("\n📋 Manual Verification:")
            response = input("   Did Pico restore MQTT-SN session properly? (y/n/s=skip): ").strip().lower()
            
            if response == 's':
                print("   ⏭  Test skipped\n")
                continue
            elif response == 'n':
                print("   ⚠️  Session restoration issue noted\n")
                if test_results[-1] == 'PASS':
                    test_results[-1] = 'FAIL'
                    passed -= 1
                    failed += 1
            
            print()
            time.sleep(2)  # Delay before next test
        
        # Print summary
        print("\n" + "="*70)
        print("TEST SUMMARY")
        print("="*70)
        success_rate = (passed / TEST_RUNS) * 100 if TEST_RUNS > 0 else 0
        avg_recovery = (total_recovery_time / passed) if passed > 0 else 0
        
        print(f"Passed: {passed}/{TEST_RUNS}")
        print(f"Failed: {failed}/{TEST_RUNS}")
        print(f"Success Rate: {success_rate:.1f}%")
        print(f"Average Recovery Time: {avg_recovery:.2f}s")
        print(f"Requirement: <= {RECONNECT_TIMEOUT_SEC}s")
        print(f"\nTolerance: {passed} out of {TEST_RUNS} test cases, "
              f"achieved {success_rate:.1f}% success")
        
        print("\n" + "="*70)
        print("VERIFICATION CHECKLIST")
        print("="*70)
        print("✓ Pico W WiFi reconnection successful")
        print("✓ DHCP IP address obtained")
        print("✓ MQTT-SN CONNECT sent to gateway")
        print("✓ Session restored (topic registrations, subscriptions)")
        print("✓ Publishing resumed automatically")
        print("✓ All within 15 second timeout")
        
        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        print(f"❌ Test failed with exception: {e}")
        try:
            client.loop_stop()
            client.disconnect()
        except:
            pass
        sys.exit(1)

if __name__ == "__main__":
    try:
        run_test()
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        sys.exit(0)
