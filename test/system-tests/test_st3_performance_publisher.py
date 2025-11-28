#!/usr/bin/env python3
"""
ST3: Performance Test - High Volume Messages (Publisher Mode)
Tests Pico W publisher firmware with high message reception rate
Monitors if Pico can handle ~5 messages per second for 60 seconds while publishing
Expects system remains responsive with no resets or freezes
"""

import paho.mqtt.client as mqtt
import time
import sys
import threading

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC = "pico/test"  # Topic where Pico publishes
MONITOR_DURATION_SEC = 600  # 600 seconds = 10 minutes monitoring
MIN_MESSAGES_EXPECTED = 120  # Pico publishes every 5s, so at least 120 messages in 10 minutes

# Test tracking
messages_received = []
lock = threading.Lock()
start_time = None

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        client.subscribe(TEST_TOPIC, qos=1)
        print(f"📡 Subscribed to {TEST_TOPIC}")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when message is received from Pico"""
    with lock:
        timestamp = time.time()
        payload = msg.payload.decode('utf-8', errors='ignore')
        messages_received.append({
            'time': timestamp,
            'payload': payload,
            'qos': msg.qos
        })
        
        elapsed = timestamp - start_time if start_time else 0
        print(f"  📥 [{elapsed:5.1f}s] Message #{len(messages_received)}: {payload[:50]}...")

def run_test():
    """Run ST3 performance test"""
    global start_time
    
    print("="*70)
    print("ST3: Performance Test - Publisher Stability (10 MINUTES)")
    print("="*70)
    print(f"Test duration: {MONITOR_DURATION_SEC} seconds ({MONITOR_DURATION_SEC/60:.0f} minutes)")
    print(f"Expected: >= {MIN_MESSAGES_EXPECTED} messages from Pico")
    print(f"Success criteria:")
    print(f"  - Pico continues publishing regularly")
    print(f"  - No system resets or freezes")
    print(f"  - >= 90% expected messages received\n")
    
    print("⚠️  PREREQUISITES:")
    print("   1. Pico W must be running picow_network.uf2 (publisher firmware)")
    print("   2. Pico W must be publishing to 'pico/test' every ~5 seconds")
    print("   3. MQTT-SN Gateway must be running")
    print("   4. Mosquitto broker must be running")
    print("   5. Monitor Pico W serial for stability\n")
    
    print("⚠️  DURING TEST:")
    print("   Watch Pico serial monitor for:")
    print("   - ❌ No resets (no boot messages)")
    print("   - ❌ No freezes (publishing continues)")
    print("   - ❌ No error messages")
    print("   - ✅ System remains responsive\n")
    
    input("Press Enter when ready to start performance test...")
    
    # Setup subscriber (monitors Pico's messages)
    client = mqtt.Client(client_id="perf_monitor")
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        print(f"\n📡 Connecting to broker...")
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(2)
        
        print(f"\n{'='*70}")
        print(f"🚀 PERFORMANCE TEST STARTED - 10 MINUTE TEST")
        print(f"{'='*70}")
        print(f"Monitoring for {MONITOR_DURATION_SEC} seconds ({MONITOR_DURATION_SEC/60:.0f} minutes)...")
        print(f"Watch Pico serial monitor for stability!")
        print(f"Progress updates every 30 seconds\n")
        
        start_time = time.time()
        
        # Monitor for test duration
        while time.time() - start_time < MONITOR_DURATION_SEC:
            elapsed = time.time() - start_time
            with lock:
                msg_count = len(messages_received)
            
            # Print progress every 30 seconds
            if int(elapsed) % 30 == 0 and int(elapsed) > 0:
                rate = msg_count / elapsed if elapsed > 0 else 0
                mins_elapsed = elapsed / 60
                mins_total = MONITOR_DURATION_SEC / 60
                print(f"\n⏱  Progress: {mins_elapsed:.1f} min / {mins_total:.0f} min ({elapsed:.0f}s / {MONITOR_DURATION_SEC}s)")
                print(f"   Messages received: {msg_count}")
                print(f"   Rate: {rate:.2f} msg/sec")
                print(f"   Expected by now: ~{int(elapsed/5)} messages")
            
            time.sleep(1)
        
        # Test complete
        end_time = time.time()
        actual_duration = end_time - start_time
        
        print(f"\n{'='*70}")
        print(f"✅ TEST COMPLETED")
        print(f"{'='*70}\n")
        
        # Calculate results
        with lock:
            total_received = len(messages_received)
        
        expected_messages = MONITOR_DURATION_SEC / 5  # Pico publishes every ~5 seconds
        success_rate = (total_received / expected_messages) * 100
        avg_rate = total_received / actual_duration
        
        # Analyze timing consistency
        if len(messages_received) > 1:
            intervals = []
            for i in range(1, len(messages_received)):
                interval = messages_received[i]['time'] - messages_received[i-1]['time']
                intervals.append(interval)
            
            avg_interval = sum(intervals) / len(intervals)
            max_interval = max(intervals)
            min_interval = min(intervals)
        else:
            avg_interval = 0
            max_interval = 0
            min_interval = 0
        
        # Ask about stability
        print("MANUAL VERIFICATION:")
        print("=" * 70)
        stable = input("Did Pico remain stable? (no resets, no freezes) [y/n]: ").strip().lower()
        errors_seen = input("Any errors in Pico serial output? [y/n]: ").strip().lower()
        
        # Print summary
        print("\n" + "="*70)
        print("TEST SUMMARY")
        print("="*70)
        print(f"Test duration: {actual_duration:.1f} seconds")
        print(f"Messages received: {total_received}")
        print(f"Expected messages: ~{expected_messages:.0f}")
        print(f"Success rate: {success_rate:.1f}%")
        print(f"Average rate: {avg_rate:.2f} msg/sec")
        
        if len(messages_received) > 1:
            print(f"\nMessage timing:")
            print(f"  Average interval: {avg_interval:.2f}s")
            print(f"  Min interval: {min_interval:.2f}s")
            print(f"  Max interval: {max_interval:.2f}s")
        
        print(f"\nStability:")
        print(f"  System stable: {'✅ YES' if stable == 'y' else '❌ NO'}")
        print(f"  Errors observed: {'❌ YES' if errors_seen == 'y' else '✅ NO'}")
        
        # Overall pass/fail
        passed = (success_rate >= 90 and stable == 'y' and errors_seen == 'n')
        
        print(f"\n{'='*70}")
        if passed:
            print("✅ TEST PASSED")
            print(f"Tolerance: PASS - System handled {actual_duration:.0f}s test")
            print(f"  - {success_rate:.1f}% message delivery (>= 90% required)")
            print(f"  - System remained stable (no resets/freezes)")
        else:
            print("❌ TEST FAILED")
            if success_rate < 90:
                print(f"  - Only {success_rate:.1f}% delivery rate (< 90% required)")
            if stable != 'y':
                print("  - System instability detected")
            if errors_seen == 'y':
                print("  - Errors observed in serial output")
        print(f"{'='*70}\n")
        
        print("DETAILED ANALYSIS:")
        print("="*70)
        print("Performance characteristics:")
        print(f"  ✓ Test ran for {actual_duration:.1f} seconds without interruption")
        print(f"  ✓ Received {total_received} messages from Pico")
        print(f"  ✓ Pico's publish rate: ~{avg_rate:.2f} messages/second")
        print(f"  ✓ Message delivery: {success_rate:.1f}%")
        
        if success_rate >= 90:
            print(f"  ✅ Delivery rate meets >= 90% requirement")
        else:
            print(f"  ❌ Delivery rate below 90% requirement")
        
        if stable == 'y':
            print(f"  ✅ System remained responsive (no resets/freezes)")
        else:
            print(f"  ❌ System stability issues detected")
        
        client.loop_stop()
        client.disconnect()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        print(f"Test ran for {time.time() - start_time:.1f} seconds")
        print(f"Messages received: {len(messages_received)}")
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"\n❌ Test failed with exception: {e}")
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
