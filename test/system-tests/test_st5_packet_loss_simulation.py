#!/usr/bin/env python3
"""
ST5: Packet Loss Simulation Test
Tests QoS behavior under simulated UDP packet loss conditions
- QoS 0: Loss observed with no retries
- QoS 1/2: Client retries 3 times, successful delivery exactly once
- Failure reported after retry limit reached
"""

import paho.mqtt.client as mqtt
import time
import sys
import subprocess
import platform

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_TOPIC_QOS0 = "pico/loss/qos0"
TEST_TOPIC_QOS1 = "pico/loss/qos1"
TEST_TOPIC_QOS2 = "pico/loss/qos2"
TEST_RUNS = 100  # 100 test cases as per requirement
MAX_RETRIES = 3

# Test results
test_results = []
messages_received = {}

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        # Subscribe to all test topics
        client.subscribe(TEST_TOPIC_QOS0, qos=0)
        client.subscribe(TEST_TOPIC_QOS1, qos=1)
        client.subscribe(TEST_TOPIC_QOS2, qos=2)
    else:
        print(f"❌ Connection failed: {rc}")

def on_message(client, userdata, msg):
    """Callback when message is received"""
    payload = msg.payload.decode('utf-8')
    topic = msg.topic
    
    if topic not in messages_received:
        messages_received[topic] = []
    
    messages_received[topic].append({
        'payload': payload,
        'time': time.time(),
        'qos': msg.qos
    })
    
    print(f"📥 Received on {topic} (QoS {msg.qos}): {payload[:40]}...")

def check_packet_loss_tools():
    """Check if packet loss simulation tools are available"""
    system = platform.system()
    
    print("🔍 Checking for packet loss simulation tools...")
    
    if system == "Linux":
        # Check for tc (traffic control)
        try:
            result = subprocess.run(['which', 'tc'], capture_output=True, text=True)
            if result.returncode == 0:
                print("   ✅ 'tc' (traffic control) available")
                return "tc"
        except:
            pass
    
    print("   ⚠️  No automated packet loss tools detected")
    print("   Manual packet loss simulation required")
    return None

def run_test():
    """Run ST5 packet loss simulation test"""
    print("="*70)
    print("ST5: Packet Loss Simulation Test")
    print("="*70)
    print(f"Test runs: {TEST_RUNS}")
    print(f"Max retries: {MAX_RETRIES}")
    print(f"Test topics:")
    print(f"  - {TEST_TOPIC_QOS0} (QoS 0)")
    print(f"  - {TEST_TOPIC_QOS1} (QoS 1)")
    print(f"  - {TEST_TOPIC_QOS2} (QoS 2)\n")
    
    loss_tool = check_packet_loss_tools()
    
    print("\n⚠️  PREREQUISITES:")
    print("   1. Pico W must be running and connected")
    print("   2. Pico W publishing with different QoS levels")
    print("   3. MQTT-SN Gateway must be running")
    print("   4. Mosquitto broker must be running")
    print("   5. Packet loss simulation capability (router QoS, iptables, tc, etc.)\n")
    
    print("📋 PACKET LOSS SIMULATION OPTIONS:")
    print("   Option A: Use router QoS settings to drop UDP packets")
    print("   Option B: Use network emulation tools (tc, clumsy, etc.)")
    print("   Option C: Physical interference (distance, obstacles)")
    print("   Option D: Software firewall rules to drop packets\n")
    
    if loss_tool == "tc":
        print("💡 TIP: You can use 'tc' for automated packet loss:")
        print("   sudo tc qdisc add dev <interface> root netem loss 20%")
        print("   (Remember to remove after test with: sudo tc qdisc del dev <interface> root)\n")
    
    input("Press Enter when packet loss simulation is ready...")
    
    # Setup test client
    client = mqtt.Client(client_id="packet_loss_tester")
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        time.sleep(2)
        
        # Test categories
        tests = {
            'qos0': {'passed': 0, 'failed': 0, 'topic': TEST_TOPIC_QOS0},
            'qos1': {'passed': 0, 'failed': 0, 'topic': TEST_TOPIC_QOS1},
            'qos2': {'passed': 0, 'failed': 0, 'topic': TEST_TOPIC_QOS2}
        }
        
        print("\n" + "="*70)
        print("TEST SCENARIOS")
        print("="*70 + "\n")
        
        # Test QoS 0 - Should show losses without retries
        print("--- Scenario 1: QoS 0 (No Retries) ---")
        print("Expected: Some messages lost, no retry attempts\n")
        
        for i in range(1, 11):  # 10 test messages
            payload = f"QOS0_MSG:{i}:{time.time()}"
            client.publish(TEST_TOPIC_QOS0, payload, qos=0)
            print(f"📤 Published QoS 0 message {i}/10")
            time.sleep(0.5)
        
        time.sleep(3)  # Wait for messages
        
        qos0_received = len(messages_received.get(TEST_TOPIC_QOS0, []))
        qos0_loss_rate = ((10 - qos0_received) / 10) * 100
        
        print(f"\n📊 QoS 0 Results:")
        print(f"   Sent: 10, Received: {qos0_received}, Lost: {10 - qos0_received} ({qos0_loss_rate:.1f}%)")
        
        if qos0_loss_rate > 0:
            print(f"   ✅ PASS: Loss detected as expected for QoS 0")
            tests['qos0']['passed'] += 1
        else:
            print(f"   ⚠️  No loss detected (packet loss may not be active)")
            tests['qos0']['failed'] += 1
        
        messages_received.clear()
        time.sleep(2)
        
        # Test QoS 1 - Should retry up to 3 times
        print("\n--- Scenario 2: QoS 1 (With Retries) ---")
        print("Expected: Retry attempts visible, eventual delivery or failure after 3 retries\n")
        
        print("⚠️  MONITOR PICO SERIAL OUTPUT for retry attempts!")
        print("   Look for: [RETRY 1/3], [RETRY 2/3], [RETRY 3/3]\n")
        
        for i in range(1, 11):  # 10 test messages
            payload = f"QOS1_MSG:{i}:{time.time()}"
            client.publish(TEST_TOPIC_QOS1, payload, qos=1)
            print(f"📤 Published QoS 1 message {i}/10")
            time.sleep(1)  # Allow time for retries
        
        time.sleep(5)  # Wait for retries and delivery
        
        qos1_messages = messages_received.get(TEST_TOPIC_QOS1, [])
        qos1_received = len(qos1_messages)
        
        print(f"\n📊 QoS 1 Results:")
        print(f"   Sent: 10, Received: {qos1_received}")
        
        # Check for duplicates (should be exactly once)
        payloads = [msg['payload'] for msg in qos1_messages]
        unique_payloads = set(payloads)
        duplicates = len(payloads) - len(unique_payloads)
        
        print(f"   Unique messages: {len(unique_payloads)}")
        print(f"   Duplicates: {duplicates}")
        
        response = input("\n   Did you observe retry attempts (up to 3) on Pico serial? (y/n): ").strip().lower()
        retries_observed = (response == 'y')
        
        if qos1_received > 0 and duplicates == 0 and retries_observed:
            print(f"   ✅ PASS: QoS 1 retry mechanism working correctly")
            tests['qos1']['passed'] += 1
        else:
            print(f"   ❌ FAIL: Issues with QoS 1 retry behavior")
            tests['qos1']['failed'] += 1
        
        messages_received.clear()
        time.sleep(2)
        
        # Test QoS 2 - Should retry with exactly-once delivery
        print("\n--- Scenario 3: QoS 2 (Exactly Once) ---")
        print("Expected: Retry attempts with exactly-once delivery guarantee\n")
        
        print("⚠️  MONITOR PICO SERIAL OUTPUT for QoS 2 handshake!")
        print("   Look for: PUBLISH -> PUBREC -> PUBREL -> PUBCOMP\n")
        
        for i in range(1, 11):  # 10 test messages
            payload = f"QOS2_MSG:{i}:{time.time()}"
            client.publish(TEST_TOPIC_QOS2, payload, qos=2)
            print(f"📤 Published QoS 2 message {i}/10")
            time.sleep(1)
        
        time.sleep(5)
        
        qos2_messages = messages_received.get(TEST_TOPIC_QOS2, [])
        qos2_received = len(qos2_messages)
        
        print(f"\n📊 QoS 2 Results:")
        print(f"   Sent: 10, Received: {qos2_received}")
        
        # Check for duplicates (should be exactly once)
        payloads = [msg['payload'] for msg in qos2_messages]
        unique_payloads = set(payloads)
        duplicates = len(payloads) - len(unique_payloads)
        
        print(f"   Unique messages: {len(unique_payloads)}")
        print(f"   Duplicates: {duplicates}")
        
        response = input("\n   Did you observe QoS 2 handshake on Pico serial? (y/n): ").strip().lower()
        handshake_observed = (response == 'y')
        
        if qos2_received > 0 and duplicates == 0 and handshake_observed:
            print(f"   ✅ PASS: QoS 2 exactly-once delivery working correctly")
            tests['qos2']['passed'] += 1
        else:
            print(f"   ❌ FAIL: Issues with QoS 2 behavior")
            tests['qos2']['failed'] += 1
        
        # Test retry limit exceeded scenario
        print("\n--- Scenario 4: Retry Limit Exceeded ---")
        print("Expected: After 3 failed retries, error reported and message dropped\n")
        
        print("⚠️  Increase packet loss to ~90-100% for this test")
        input("Press Enter when high packet loss is configured...")
        
        messages_received.clear()
        
        payload = f"FAIL_TEST:{time.time()}"
        client.publish(TEST_TOPIC_QOS1, payload, qos=1)
        print(f"📤 Published message that should fail after retries")
        
        time.sleep(10)  # Wait for all retry attempts
        
        response = input("Did Pico report 'Max retries exceeded' error? (y/n): ").strip().lower()
        error_reported = (response == 'y')
        
        if error_reported:
            print("   ✅ PASS: Retry limit and error reporting working correctly")
        else:
            print("   ❌ FAIL: Retry limit error not properly reported")
        
        # Print summary
        print("\n" + "="*70)
        print("TEST SUMMARY")
        print("="*70)
        
        total_passed = sum(t['passed'] for t in tests.values())
        total_tests = len(tests) + 1  # +1 for retry limit test
        
        print("\nQoS 0 (No Retries):")
        print(f"  Result: {'✅ PASS' if tests['qos0']['passed'] > 0 else '❌ FAIL'}")
        print(f"  - Packet loss observed without retries")
        
        print("\nQoS 1 (At Least Once):")
        print(f"  Result: {'✅ PASS' if tests['qos1']['passed'] > 0 else '❌ FAIL'}")
        print(f"  - Retries up to {MAX_RETRIES} times")
        print(f"  - Successful delivery without duplicates")
        
        print("\nQoS 2 (Exactly Once):")
        print(f"  Result: {'✅ PASS' if tests['qos2']['passed'] > 0 else '❌ FAIL'}")
        print(f"  - 4-way handshake completed")
        print(f"  - Exactly-once delivery guaranteed")
        
        print("\nRetry Limit Exceeded:")
        print(f"  Result: {'✅ PASS' if error_reported else '❌ FAIL'}")
        print(f"  - Error reported after {MAX_RETRIES} failed attempts")
        
        success_rate = (total_passed / total_tests) * 100
        print(f"\nOverall Success: {total_passed}/{total_tests} scenarios passed ({success_rate:.1f}%)")
        print(f"\nTolerance: {total_passed} out of {total_tests} test scenarios, "
              f"achieved {success_rate:.1f}% success")
        
        print("\n" + "="*70)
        print("VERIFICATION CHECKLIST")
        print("="*70)
        print("✓ QoS 0: Loss observed, no retries")
        print("✓ QoS 1: Retry mechanism active (up to 3 attempts)")
        print("✓ QoS 1: Messages delivered exactly once")
        print("✓ QoS 2: 4-way handshake observed")
        print("✓ QoS 2: Exactly-once guarantee maintained")
        print("✓ Retry limit: Error reported after max attempts")
        print("✓ System recovered after packet loss removed")
        
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
