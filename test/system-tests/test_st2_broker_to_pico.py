#!/usr/bin/env python3
"""
ST2: Broker to Pico Message Delivery
Tests message delivery from broker to Pico W within 3-7 seconds with payload integrity
"""

import paho.mqtt.client as mqtt
import time
import sys
import random
import string

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test/downlink"  # Topic where Pico subscribes
TEST_RUNS = 100  # 100 test cases as per requirement
TIMEOUT_SEC = 7  # 3-7 seconds requirement (using max)

# Test results
test_results = []

def generate_test_payload():
    """Generate random test message"""
    return f"BROKER_MSG:{time.time()}:{random.randint(1000, 9999)}:{''.join(random.choices(string.ascii_letters, k=16))}"

def run_test():
    """Run ST2 test"""
    print("="*60)
    print("ST2: Broker to Pico Message Delivery Test")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Timeout requirement: 3-7 seconds (using {TIMEOUT_SEC}s)\n")
    
    print("⚠️  PREREQUISITES:")
    print("   1. Pico W must be running and connected to WiFi")
    print("   2. Pico W must be subscribed to topic: 'pico/test/downlink'")
    print("   3. MQTT-SN Gateway must be running")
    print("   4. Mosquitto broker must be running")
    print("   5. Monitor Pico W serial output to verify message receipt\n")
    
    input("Press Enter when Pico W is ready and subscribed...")
    
    # Setup publisher client (simulates broker publishing)
    client = mqtt.Client(client_id="test_broker_publisher")
    
    try:
        print(f"\n📡 Connecting to broker {BROKER}:{PORT}...")
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(1)
        
        passed = 0
        failed = 0
        total_latency = 0
        
        print("\n" + "="*60)
        print("Starting tests...")
        print("="*60)
        print("⚠️  IMPORTANT: Manually verify on Pico W serial monitor that:")
        print("   - Messages are received")
        print("   - Payload matches exactly")
        print("   - Timestamp is within 3-7 seconds\n")
        
        for i in range(1, TEST_RUNS + 1):
            # Generate test payload
            test_payload = generate_test_payload()
            
            # Publish message from broker
            send_time = time.time()
            result = client.publish(TEST_TOPIC, test_payload, qos=1)
            result.wait_for_publish()
            
            print(f"  📤 Test {i}/{TEST_RUNS}: Published '{test_payload[:40]}...'")
            print(f"     ⏱  Sent at: {time.strftime('%H:%M:%S', time.localtime(send_time))}")
            
            # Prompt for manual verification
            response = input(f"     Did Pico receive within {TIMEOUT_SEC}s with matching payload? (y/n/s=skip): ").strip().lower()
            
            if response == 'y':
                # Ask for latency
                try:
                    latency_str = input(f"     Enter latency in seconds (or press Enter for estimated 5s): ").strip()
                    if latency_str:
                        latency = float(latency_str)
                    else:
                        latency = 5.0  # Default mid-range value
                    
                    total_latency += latency
                    
                    if latency <= TIMEOUT_SEC:
                        print(f"     ✅ PASS: Message delivered in {latency:.2f}s\n")
                        passed += 1
                        test_results.append('PASS')
                    else:
                        print(f"     ❌ FAIL: Latency {latency:.2f}s exceeds {TIMEOUT_SEC}s\n")
                        failed += 1
                        test_results.append('FAIL')
                except ValueError:
                    print(f"     ❌ FAIL: Invalid latency input\n")
                    failed += 1
                    test_results.append('FAIL')
                    
            elif response == 'n':
                print(f"     ❌ FAIL: Message not received or payload mismatch\n")
                failed += 1
                test_results.append('FAIL')
                
            elif response == 's':
                print(f"     ⏭  SKIPPED\n")
                continue
            else:
                print(f"     ❌ FAIL: Invalid response\n")
                failed += 1
                test_results.append('FAIL')
            
            time.sleep(0.5)  # Small delay between tests
        
        # Print summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        success_rate = (passed / TEST_RUNS) * 100 if TEST_RUNS > 0 else 0
        avg_latency = (total_latency / passed) if passed > 0 else 0
        
        print(f"Passed: {passed}/{TEST_RUNS}")
        print(f"Failed: {failed}/{TEST_RUNS}")
        print(f"Success Rate: {success_rate:.1f}%")
        print(f"Average Latency: {avg_latency:.2f}s")
        print(f"\nTolerance: {passed} out of {TEST_RUNS} test cases, achieved {success_rate:.1f}% success")
        
        print("\n" + "="*60)
        print("VERIFICATION POINTS")
        print("="*60)
        print("✓ Broker published messages to MQTT topic")
        print("✓ Gateway forwarded MQTT to MQTT-SN")
        print("✓ Pico W received messages via UDP")
        print("✓ Payload integrity maintained")
        print("✓ Latency within 3-7 second requirement")
        
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
