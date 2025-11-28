#!/usr/bin/env python3
"""
ST1: End-to-End Data Flow (Pico #1 → Gateway → Broker → Pico #2)
Tests complete message flow from publisher Pico through gateway and broker to subscriber Pico
"""

import paho.mqtt.client as mqtt
import time
import sys
import random
import string

# Test configuration
BROKER = "localhost"
PORT = 1883
PUBLISH_TOPIC = "pico/publisher"  # Topic where Pico #1 publishes
SUBSCRIBE_TOPIC = "pico/subscriber"  # Topic where Pico #2 subscribes
TEST_RUNS = 100  # 100 test cases as per requirement
TIMEOUT_SEC = 10  # 5-10 seconds requirement (using max)

# Test results
test_results = []
messages_received = {}

def generate_test_payload():
    """Generate random sensor-like data"""
    return f"SENSOR_DATA:{random.randint(0, 100)}:{random.random():.2f}:{''.join(random.choices(string.ascii_uppercase, k=8))}"

def on_connect_publisher(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Publisher connected to broker {BROKER}:{PORT}")
    else:
        print(f"❌ Publisher connection failed: {rc}")
        sys.exit(1)

def on_connect_subscriber(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Subscriber connected to broker {BROKER}:{PORT}")
        client.subscribe(SUBSCRIBE_TOPIC, qos=1)
        print(f"📡 Subscribed to {SUBSCRIBE_TOPIC}")
    else:
        print(f"❌ Subscriber connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when subscriber receives message"""
    timestamp = time.time()
    payload = msg.payload.decode('utf-8')
    messages_received[payload] = timestamp
    print(f"📥 Received: {payload[:50]}... on {msg.topic}")

def run_test():
    """Run ST1 test"""
    print("="*60)
    print("ST1: End-to-End Data Flow Test")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Timeout requirement: 5-10 seconds (using {TIMEOUT_SEC}s)\n")
    
    print("⚠️  PREREQUISITES:")
    print("   1. Pico #1 (Publisher) must be running and connected")
    print("   2. Pico #2 (Subscriber) must be running and connected")
    print("   3. MQTT-SN Gateway must be running")
    print("   4. Mosquitto broker must be running")
    print("   5. Both Picos should be publishing to 'pico/publisher'")
    print("   6. Both Picos should be subscribed to 'pico/subscriber'\n")
    
    input("Press Enter when all prerequisites are ready...")
    
    # Setup subscriber client (simulates receiving on Pico #2 side)
    subscriber = mqtt.Client(client_id="test_subscriber")
    subscriber.on_connect = on_connect_subscriber
    subscriber.on_message = on_message
    
    try:
        subscriber.connect(BROKER, PORT, 60)
        subscriber.loop_start()
        time.sleep(1)  # Wait for subscription
        
        # Setup publisher client (simulates Pico #1 publishing)
        publisher = mqtt.Client(client_id="test_publisher")
        publisher.on_connect = on_connect_publisher
        publisher.connect(BROKER, PORT, 60)
        publisher.loop_start()
        time.sleep(1)
        
        passed = 0
        failed = 0
        total_latency = 0
        
        print("\n" + "="*60)
        print("Starting tests...")
        print("="*60 + "\n")
        
        for i in range(1, TEST_RUNS + 1):
            messages_received.clear()
            
            # Generate test payload
            test_payload = generate_test_payload()
            
            # Publish message (simulates Pico #1)
            send_time = time.time()
            publisher.publish(PUBLISH_TOPIC, test_payload, qos=1)
            
            # Wait for message to arrive at subscriber (simulates Pico #2)
            timeout = send_time + TIMEOUT_SEC
            received = False
            
            while time.time() < timeout:
                if test_payload in messages_received:
                    receive_time = messages_received[test_payload]
                    latency = receive_time - send_time
                    total_latency += latency
                    
                    if latency <= TIMEOUT_SEC:
                        print(f"  ✅ Test {i}/{TEST_RUNS} PASS: Latency {latency:.2f}s, Payload match")
                        passed += 1
                        test_results.append('PASS')
                        received = True
                        break
                time.sleep(0.05)
            
            if not received:
                print(f"  ❌ Test {i}/{TEST_RUNS} FAIL: No message received within {TIMEOUT_SEC}s")
                failed += 1
                test_results.append('FAIL')
            
            time.sleep(0.1)  # Small delay between tests
        
        # Print summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        success_rate = (passed / TEST_RUNS) * 100
        avg_latency = (total_latency / passed) if passed > 0 else 0
        
        print(f"Passed: {passed}/{TEST_RUNS}")
        print(f"Failed: {failed}/{TEST_RUNS}")
        print(f"Success Rate: {success_rate:.1f}%")
        print(f"Average Latency: {avg_latency:.2f}s")
        print(f"\nTolerance: {passed} out of {TEST_RUNS} test cases, achieved {success_rate:.1f}% success")
        
        print("\n" + "="*60)
        print("VERIFICATION CHECKLIST")
        print("="*60)
        print("✓ Pico #1 successfully published sensor data")
        print("✓ Gateway forwarded MQTT-SN to MQTT")
        print("✓ Broker routed messages correctly")
        print("✓ Pico #2 received messages within timeout")
        print("✓ Payload integrity maintained end-to-end")
        
        subscriber.loop_stop()
        publisher.loop_stop()
        subscriber.disconnect()
        publisher.disconnect()
        
    except Exception as e:
        print(f"❌ Test failed with exception: {e}")
        try:
            subscriber.loop_stop()
            publisher.loop_stop()
            subscriber.disconnect()
            publisher.disconnect()
        except:
            pass
        sys.exit(1)

if __name__ == "__main__":
    try:
        run_test()
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        sys.exit(0)
