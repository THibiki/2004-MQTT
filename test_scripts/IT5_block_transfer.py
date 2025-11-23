#!/usr/bin/env python3
"""
IT5: Successful Block Transfer test
Tests if all fragments are received and reassembled correctly
Based on existing receive_blocks.py code
"""

import paho.mqtt.client as mqtt
import struct
import time
import sys
import hashlib

# Test configuration
BROKER = "localhost"
PORT = 1883
TEST_RUNS = 10  # 10 block transfers (press GP21 button 10 times)

# Test results
test_results = []
current_block = {
    'id': None,
    'total_parts': 0,
    'parts': {},
    'start_time': None,
    'expected_checksum': None
}

def calculate_checksum(data):
    """Calculate SHA256 checksum of data"""
    return hashlib.sha256(data).hexdigest()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker {BROKER}:{PORT}")
        client.subscribe("pico/chunks")
        client.subscribe("pico/block")
        print("📡 Subscribed to pico/chunks and pico/block\n")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    """Process received messages"""
    global current_block
    
    # Handle completion notifications
    if msg.topic == "pico/block":
        notification = msg.payload.decode('utf-8', errors='ignore')
        print(f"📬 {notification}")
        return
    
    if len(msg.payload) < 8:
        return
    
    try:
        # Parse header: block_id (2), part_num (2), total_parts (2), data_len (2)
        block_id, part_num, total_parts, data_len = struct.unpack('<HHHH', msg.payload[:8])
        chunk_data = msg.payload[8:8+data_len]
        
        if len(chunk_data) != data_len:
            return
        
        # New block started
        if current_block['id'] != block_id:
            if current_block['id'] is not None:
                check_block_completion()
            
            current_block = {
                'id': block_id,
                'total_parts': total_parts,
                'parts': {},
                'start_time': time.time(),
                'expected_checksum': None
            }
            print(f"\n🆕 Test #{len(test_results)+1}: Receiving block {block_id} ({total_parts} parts)")
        
        # Store chunk
        if part_num not in current_block['parts']:
            current_block['parts'][part_num] = chunk_data
            received = len(current_block['parts'])
            
            if received % 10 == 0 or received == total_parts:
                progress = (received / total_parts) * 100
                print(f"  📦 Progress: {received}/{total_parts} ({progress:.0f}%)")
        
        # Check if complete
        if len(current_block['parts']) == total_parts:
            check_block_completion()
            
    except Exception as e:
        print(f"❌ Error processing message: {e}")

def check_block_completion():
    """Verify block reassembly"""
    global current_block, test_results
    
    # Check for missing chunks
    missing = []
    for i in range(1, current_block['total_parts'] + 1):
        if i not in current_block['parts']:
            missing.append(i)
    
    if missing:
        print(f"  ❌ FAIL: Missing chunks {missing}")
        test_results.append('FAIL')
        return
    
    # Reassemble
    image_data = bytearray()
    for i in range(1, current_block['total_parts'] + 1):
        image_data.extend(current_block['parts'][i])
    
    if len(image_data) == 0:
        print(f"  ❌ FAIL: No data received")
        test_results.append('FAIL')
        return
    
    # Calculate checksum
    checksum = calculate_checksum(bytes(image_data))
    elapsed = time.time() - current_block['start_time']
    size_kb = len(image_data) / 1024
    
    # Verify file type
    valid_image = False
    if image_data[:2] == b'\xff\xd8':
        valid_image = True
        file_type = "JPEG"
    elif image_data[:2] == b'\x89\x50':
        valid_image = True
        file_type = "PNG"
    elif image_data[:2] == b'\x47\x49':
        valid_image = True
        file_type = "GIF"
    else:
        file_type = "Unknown"
    
    # Check results
    if valid_image:
        print(f"  ✅ PASS: Block {current_block['id']} complete")
        print(f"     - Size: {size_kb:.2f} KB")
        print(f"     - Type: {file_type}")
        print(f"     - Time: {elapsed:.2f}s")
        print(f"     - Checksum: {checksum[:16]}...")
        print(f"     - All {current_block['total_parts']} fragments received")
        test_results.append('PASS')
    else:
        print(f"  ⚠️  WARN: Block complete but not a valid image")
        print(f"     - Size: {size_kb:.2f} KB")
        print(f"     - Type: {file_type}")
        test_results.append('WARN')

def run_test():
    """Run IT5 test"""
    print("="*60)
    print("IT5: Block Transfer Test")
    print("="*60)
    print(f"Target: {TEST_RUNS} successful transfers")
    print("Waiting for block transfers from Pico W...")
    print("(Press GP21 button on Pico to start transfer)\n")
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        
        # Wait for connection
        time.sleep(2)
        
        # Run until we get enough tests or timeout
        start_time = time.time()
        timeout = 600  # 10 minutes max
        
        while len(test_results) < TEST_RUNS and (time.time() - start_time) < timeout:
            time.sleep(1)
        
        # Print summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)
        
        passed = test_results.count('PASS')
        failed = test_results.count('FAIL')
        warned = test_results.count('WARN')
        total = len(test_results)
        
        if total > 0:
            success_rate = (passed / total) * 100
            print(f"Passed: {passed}/{total}")
            print(f"Failed: {failed}/{total}")
            print(f"Warned: {warned}/{total}")
            print(f"Success Rate: {success_rate:.1f}%")
            print(f"\nTolerance: {passed} out of {total} test cases, achieved {success_rate:.1f}% success")
        else:
            print("⚠️  No block transfers received")
            print("   Press GP21 button on Pico W to trigger transfers")
        
        client.loop_stop()
        client.disconnect()
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Test interrupted by user")
        if len(test_results) > 0:
            passed = test_results.count('PASS')
            print(f"\nPartial results: {passed}/{len(test_results)} passed")
        client.loop_stop()
        client.disconnect()
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    run_test()
