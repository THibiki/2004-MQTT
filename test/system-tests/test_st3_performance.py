#!/usr/bin/env python3
"""
ST3: Performance Test - High Volume Messages
Tests system with ~5 messages per second for 60 seconds (300 total messages)
Expects >= 90% delivery rate with no system resets or freezes
"""

import paho.mqtt.client as mqtt
import time
import sys
import threading

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/performance/test"
MESSAGES_PER_SECOND = 5
TEST_DURATION_SEC = 60
TOTAL_MESSAGES = MESSAGES_PER_SECOND * TEST_DURATION_SEC  # 300 messages
SUCCESS_THRESHOLD = 0.90  # 90% success rate required

# Test tracking
messages_sent = {}  # {msg_id: send_time}
messages_received = {}  # {msg_id: receive_time}
lock = threading.Lock()

def on_connect_publisher(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Publisher connected to broker {BROKER}:{PORT}")
    else:
        print(f"❌ Publisher connection failed: {rc}")
        sys.exit(1)

def on_connect_subscriber(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Subscriber connected to broker {BROKER}:{PORT}")
        client.subscribe(TEST_TOPIC, qos=1)
        print(f"📡 Subscribed to {TEST_TOPIC}")
    else:
        print(f"❌ Subscriber connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received"""
    try:
        payload = msg.payload.decode('utf-8')
        # Extract message ID
        if payload.startswith("PERF_MSG:"):
            parts = payload.split(":")
            if len(parts) >= 2:
                msg_id = int(parts[1])
                with lock:
                    messages_received[msg_id] = time.time()
    except Exception as e:
        print(f"⚠️  Error parsing message: {e}")

def run_test():
    """Run ST3 performance test"""
    print("="*70)
    print("ST3: Performance Test - High Volume Message Delivery")
    print("="*70)
    print(f"Message rate: {MESSAGES_PER_SECOND} msg/sec")
    print(f"Test duration: {TEST_DURATION_SEC} seconds")
    print(f"Total messages: {TOTAL_MESSAGES}")
    print(f"Success threshold: {SUCCESS_THRESHOLD*100}%")
    print(f"QoS Level: 1\n")
    
    print("⚠️  PREREQUISITES:")
    print("   1. Pico W must be running and connected")
    print("   2. Pico W must be subscribed to 'pico/performance/test'")
    print("   3. MQTT-SN Gateway must be running")
    print("   4. Mosquitto broker must be running")
    print("   5. Monitor Pico W for stability (no resets/freezes)\n")
    
    input("Press Enter when ready to start performance test...")
    
    # Setup subscriber (monitors received messages)
    subscriber = mqtt.Client(client_id="perf_subscriber")
    subscriber.on_connect = on_connect_subscriber
    subscriber.on_message = on_message
    
    try:
        subscriber.connect(BROKER, PORT, 60)
        subscriber.loop_start()
        time.sleep(1)
        
        # Setup publisher
        publisher = mqtt.Client(client_id="perf_publisher")
        publisher.on_connect = on_connect_publisher
        publisher.connect(BROKER, PORT, 60)
        publisher.loop_start()
        time.sleep(1)
        
        print("\n" + "="*70)
        print("Starting performance test...")
        print("="*70 + "\n")
        
        start_time = time.time()
        message_interval = 1.0 / MESSAGES_PER_SECOND  # 0.2 seconds
        
        # Send messages at specified rate
        for i in range(TOTAL_MESSAGES):
            msg_id = i + 1
            payload = f"PERF_MSG:{msg_id}:{time.time()}:DATA"
            
            # Send message
            send_time = time.time()
            with lock:
                messages_sent[msg_id] = send_time
            
            publisher.publish(TEST_TOPIC, payload, qos=1)
            
            # Progress update every 30 messages
            if msg_id % 30 == 0:
                elapsed = time.time() - start_time
                rate = msg_id / elapsed
                with lock:
                    received_count = len(messages_received)
                delivery_rate = (received_count / msg_id) * 100 if msg_id > 0 else 0
                print(f"  📊 Progress: {msg_id}/{TOTAL_MESSAGES} sent, "
                      f"{received_count} received ({delivery_rate:.1f}%), "
                      f"Rate: {rate:.1f} msg/s")
            
            # Wait for next interval
            next_send = start_time + (msg_id * message_interval)
            sleep_time = next_send - time.time()
            if sleep_time > 0:
                time.sleep(sleep_time)
        
        # Wait for remaining messages to arrive (grace period)
        print(f"\n⏳ All messages sent. Waiting 10 seconds for remaining deliveries...")
        time.sleep(10)
        
        # Calculate results
        end_time = time.time()
        total_duration = end_time - start_time
        
        with lock:
            sent_count = len(messages_sent)
            received_count = len(messages_received)
        
        delivery_rate = (received_count / sent_count) * 100 if sent_count > 0 else 0
        actual_rate = sent_count / total_duration
        
        # Calculate latencies for received messages
        latencies = []
        with lock:
            for msg_id, recv_time in messages_received.items():
                if msg_id in messages_sent:
                    latency = recv_time - messages_sent[msg_id]
                    latencies.append(latency)
        
        avg_latency = sum(latencies) / len(latencies) if latencies else 0
        max_latency = max(latencies) if latencies else 0
        min_latency = min(latencies) if latencies else 0
        
        # Determine pass/fail
        passed = delivery_rate >= (SUCCESS_THRESHOLD * 100)
        
        # Print summary
        print("\n" + "="*70)
        print("TEST SUMMARY")
        print("="*70)
        print(f"Messages Sent: {sent_count}")
        print(f"Messages Received: {received_count}")
        print(f"Messages Lost: {sent_count - received_count}")
        print(f"Delivery Rate: {delivery_rate:.2f}%")
        print(f"Test Duration: {total_duration:.2f}s")
        print(f"Actual Send Rate: {actual_rate:.2f} msg/s")
        print(f"\nLatency Statistics:")
        print(f"  Average: {avg_latency*1000:.2f}ms")
        print(f"  Min: {min_latency*1000:.2f}ms")
        print(f"  Max: {max_latency*1000:.2f}ms")
        
        print(f"\nResult: {'✅ PASS' if passed else '❌ FAIL'}")
        print(f"Threshold: >= {SUCCESS_THRESHOLD*100}% delivery required")
        print(f"Achieved: {delivery_rate:.2f}% delivery")
        
        print("\n" + "="*70)
        print("SYSTEM STABILITY CHECK")
        print("="*70)
        print("⚠️  MANUAL VERIFICATION REQUIRED:")
        print("   1. Did Pico W remain responsive throughout the test?")
        print("   2. Were there any system resets or freezes?")
        print("   3. Did the Pico W serial output show continuous operation?")
        print("   4. Was memory usage stable (no leaks)?")
        
        response = input("\nDid the system remain stable? (y/n): ").strip().lower()
        system_stable = (response == 'y')
        
        final_result = passed and system_stable
        
        print("\n" + "="*70)
        print("FINAL RESULT")
        print("="*70)
        print(f"Delivery Rate: {'✅ PASS' if passed else '❌ FAIL'} ({delivery_rate:.2f}%)")
        print(f"System Stability: {'✅ PASS' if system_stable else '❌ FAIL'}")
        print(f"Overall: {'✅ PASS' if final_result else '❌ FAIL'}")
        print(f"\nTolerance: {received_count} out of {sent_count} test cases, "
              f"achieved {delivery_rate:.1f}% success")
        
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
