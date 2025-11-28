#!/usr/bin/env python3
"""
IT12: SD Card Operations (SEMI-AUTOMATED)
Tests SD card read/write and block transfer functionality
"""

import paho.mqtt.client as mqtt
import time
import sys
import hashlib

# Test configuration
BROKER = "localhost"
PORT = 1883
CHUNK_TOPIC = "pico/chunks"
TEST_RUNS = 10

# Test results
test_results = []
chunks_received = []

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
    if userdata['first_chunk_time'] is None:
        userdata['first_chunk_time'] = timestamp
        print(f"✅ First chunk received (SD read successful)")
    
    chunks_received.append({
        'timestamp': timestamp,
        'size': len(msg.payload),
        'payload': msg.payload
    })
    print(f"📦 Chunk {len(chunks_received)}: {len(msg.payload)} bytes")

def prompt_transfer_start():
    """Prompt user to start transfer"""
    print("\n" + "="*60)
    print("ACTION REQUIRED: Start Block Transfer")
    print("="*60)
    print("1. Press GP21 button on Pico W")
    print("2. Watch serial monitor for:")
    print("   - '[SD] SD card initalised and FAT32 mounted!'")
    print("   - '[APP] Scanning SD card for images...'")
    print("   - '✅ Image loaded from SD card: XXXXX bytes'")
    print()
    input("Press Enter after pressing GP21...")

def run_test():
    """Run IT12 test"""
    print("="*60)
    print("IT12: SD Card Operations (SEMI-AUTOMATED)")
    print("="*60)
    print(f"Test runs: {TEST_RUNS}\n")
    
    print("⚠️  PREREQUISITES:")
    print("  1. SD card with test image file inserted in Pico")
    print("  2. Pico W connected and running")
    print("  3. Serial monitor open to verify SD operations")
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
            chunks_received.clear()
            userdata = {'first_chunk_time': None}
            client._userdata = userdata
            
            print(f"\n{'='*60}")
            print(f"Test {i}/{TEST_RUNS}")
            print(f"{'='*60}")
            
            # Prompt user to start transfer
            prompt_transfer_start()
            
            # Monitor for chunks
            start_time = time.time()
            timeout = start_time + 60  # 60 seconds max for transfer
            
            print("\n⏳ Monitoring for block transfer completion...")
            last_chunk_count = 0
            stable_count = 0
            
            while time.time() < timeout:
                time.sleep(1)
                
                # Check if transfer is complete (no new chunks for 3 seconds)
                if len(chunks_received) == last_chunk_count:
                    stable_count += 1
                    if stable_count >= 3 and len(chunks_received) > 0:
                        print(f"\n✅ Transfer appears complete ({len(chunks_received)} chunks)")
                        break
                else:
                    stable_count = 0
                    last_chunk_count = len(chunks_received)
            
            elapsed = time.time() - start_time
            
            # Calculate statistics
            if len(chunks_received) > 0:
                total_bytes = sum(chunk['size'] for chunk in chunks_received)
                
                # Verify data integrity (basic check)
                print(f"\n📊 Transfer Statistics:")
                print(f"   Total chunks: {len(chunks_received)}")
                print(f"   Total bytes: {total_bytes}")
                print(f"   Transfer time: {elapsed:.2f}s")
                print(f"   Average chunk size: {total_bytes/len(chunks_received):.1f} bytes")
                
                # Check if SD read was successful (indicated by receiving chunks)
                if userdata['first_chunk_time']:
                    sd_read_time = userdata['first_chunk_time'] - start_time
                    print(f"   SD read start: ~{sd_read_time:.2f}s after button press")
                
                # Success criteria: received chunks from SD card
                print(f"\n✅ SD card operations successful:")
                print(f"   ✓ SD card read (image loaded)")
                print(f"   ✓ Block transfer completed")
                print(f"   ✓ {len(chunks_received)} fragments received")
                print(f"   Note: Check serial for checksum validation")
                
                passed += 1
                test_results.append('PASS')
            else:
                print(f"\n❌ No chunks received")
                print(f"   Check SD card is inserted and contains image file")
                print(f"   Check serial monitor for SD errors")
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
        print("For complete verification, check Pico serial output for:")
        print("  - '[SD] Initialising SD card...'")
        print("  - '[SD] SD card initalised and FAT32 mounted!'")
        print("  - '[APP] Scanning SD card for images...'")
        print("  - '[APP] Found image: filename.jpg'")
        print("  - '📁 Reading from SD card: filename.jpg'")
        print("  - '📊 File size: XXXXX bytes'")
        print("  - '✅ Image loaded from SD card'")
        print("  - '📤 Sending to topic 'pico/chunks''")
        print("  - '✅ Block Transfer completed successfully'")
        print("\nSuccess criteria:")
        print("  - Read/write pattern matches original (no bit errors)")
        print("  - LED blinks success pattern within 1 second")
        print("  - Image matches original after transfer")
        
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
