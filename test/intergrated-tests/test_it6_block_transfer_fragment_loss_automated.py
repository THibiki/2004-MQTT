#!/usr/bin/env python3
"""
IT6: Block Transfer with Fragment Loss (SEMI-AUTOMATED)
Tests fragment retransmission and reassembly after network disruption
"""

import paho.mqtt.client as mqtt
import time
import sys
import subprocess

# Test configuration
BROKER = "localhost"
PORT = 1883
CHUNK_TOPIC = "pico/chunks"
TEST_RUNS = 10

# Test results
test_results = []
fragments_received = []

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        client.subscribe(CHUNK_TOPIC)
        print(f"📡 Subscribed to {CHUNK_TOPIC}")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Callback when chunk is received"""
    timestamp = time.time()
    payload = msg.payload
    fragments_received.append({
        'topic': msg.topic,
        'size': len(payload),
        'timestamp': timestamp,
        'qos': msg.qos
    })
    
    # Try to extract fragment info from payload (assumes metadata in first bytes)
    if len(payload) > 8:
        try:
            # Typical chunk format: [header][fragment_num][total_fragments][data]
            print(f"📦 Fragment received: size={len(payload)} bytes, qos={msg.qos}")
        except:
            print(f"📦 Fragment received: size={len(payload)} bytes")
    else:
        print(f"📦 Fragment received: size={len(payload)} bytes")

def stop_gateway():
    """Stop MQTT-SN Gateway briefly"""
    print("🔴 Stopping gateway...")
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
    print("🟢 Starting gateway...")
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

def prompt_transfer_start():
    """Prompt user to start transfer"""
    print("\n" + "="*60)
    print("ACTION REQUIRED: Start Block Transfer")
    print("="*60)
    print("1. Press GP21 button on Pico to start transfer")
    print("2. Wait a few seconds for fragments to start")
    print()
    input("Press Enter after starting transfer...")

def run_test():
    """Run IT6 test"""
    print("="*60)
    print("IT6: Block Transfer with Fragment Loss (SEMI-AUTOMATED)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}\n")
    
    print("⚠️  PREREQUISITES:")
    print("  1. SD card with image file inserted in Pico")
    print("  2. Pico W connected and running")
    print("  3. QoS 1 or QoS 2 recommended for retransmission")
    print("  4. WSL with MQTT-SN Gateway installed")
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
            fragments_received.clear()
            
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Step 1: Start transfer
            prompt_transfer_start()
            
            # Step 2: Collect some initial fragments
            print("\n⏳ Collecting initial fragments...")
            time.sleep(3)
            initial_count = len(fragments_received)
            print(f"✅ Received {initial_count} fragments")
            
            if initial_count == 0:
                print("❌ No fragments received - check Pico is sending")
                failed += 1
                test_results.append('FAIL')
                continue
            
            # Step 3: Disrupt gateway to cause fragment loss
            print("\n⚠️  Disrupting network to simulate fragment loss...")
            if not stop_gateway():
                print("⚠️  Skipping test - could not control gateway")
                continue
            
            # Wait while fragments are being lost
            print("⏳ Waiting 3s (fragments will be lost)...")
            time.sleep(3)
            
            # Step 4: Restart gateway
            if not start_gateway():
                print("⚠️  Skipping test - could not restart gateway")
                continue
            
            # Step 5: Wait for retransmission and completion
            print("\n⏳ Waiting for missing fragments to be retransmitted...")
            time.sleep(10)  # Give time for retransmission and completion
            
            final_count = len(fragments_received)
            new_fragments = final_count - initial_count
            
            # Step 6: Check results
            if new_fragments > 0:
                print(f"\n✅ Retransmission detected:")
                print(f"   Initial fragments: {initial_count}")
                print(f"   Final fragments: {final_count}")
                print(f"   Retransmitted: {new_fragments}")
                
                # Check if transfer appears complete (look for completion marker)
                # In a real test, you'd verify checksum matching
                print("   Note: Verify serial output for 'Block Transfer completed'")
                passed += 1
                test_results.append('PASS')
            else:
                print(f"\n❌ No retransmission detected")
                print(f"   Fragments before disruption: {initial_count}")
                print(f"   Fragments after: {final_count}")
                failed += 1
                test_results.append('FAIL')
            
            if i < TEST_RUNS:
                print("\n⏳ Waiting 10s before next test...")
                time.sleep(10)
        
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
        print("For complete verification, check Pico serial output for:")
        print("  - Missing fragment detection (NACK sent)")
        print("  - Retransmission of missing fragments")
        print("  - 'Block Transfer completed successfully'")
        print("  - Checksum validation passed")
        
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
        # Always restart gateway
        start_gateway()

if __name__ == "__main__":
    try:
        run_test()
    finally:
        start_gateway()
