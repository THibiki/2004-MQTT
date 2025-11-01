#!/usr/bin/env python3
"""
Image Receiver - Reassembles images sent via MQTT-SN block transfer from Pico W
"""

import paho.mqtt.client as mqtt
import struct
import time
from datetime import datetime

class ImageReceiver:
    def __init__(self, broker_host="localhost", broker_port=1883):
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client = mqtt.Client()
        
        # Block reassembly state
        self.current_block = None
        self.block_id = None
        self.total_parts = 0
        self.received_parts = 0
        self.parts_data = {}
        self.start_time = None
        
        # Setup callbacks
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            print("✅ Connected to MQTT broker")
            # Subscribe to block transfer topics
            client.subscribe("pico/chunks")
            client.subscribe("pico/block")
            print("📡 Subscribed to pico/chunks and pico/block")
        else:
            print(f"❌ Connection failed with code {rc}")
    
    def on_message(self, client, userdata, msg):
        """Process incoming block transfer chunk"""
        try:
            # Parse block header (struct format: HHHH = 4 unsigned shorts)
            if len(msg.payload) < 8:
                print(f"⚠️  Packet too small: {len(msg.payload)} bytes")
                return
            
            # Unpack header: block_id, part_num, total_parts, data_len
            block_id, part_num, total_parts, data_len = struct.unpack('<HHHH', msg.payload[:8])
            chunk_data = msg.payload[8:8+data_len]
            
            # Validate chunk
            if len(chunk_data) != data_len:
                print(f"⚠️  Data length mismatch: expected {data_len}, got {len(chunk_data)}")
                return
            
            # New block starting
            if self.block_id != block_id:
                if self.block_id is not None:
                    print(f"⚠️  New block {block_id} started before block {self.block_id} completed")
                
                self.block_id = block_id
                self.total_parts = total_parts
                self.received_parts = 0
                self.parts_data = {}
                self.start_time = time.time()
                
                print(f"\n{'='*60}")
                print(f"🆕 Starting new block transfer:")
                print(f"   Block ID: {block_id}")
                print(f"   Total parts: {total_parts}")
                print(f"   Topic: {msg.topic}")
                print(f"{'='*60}")
            
            # Store chunk
            if part_num not in self.parts_data:
                self.parts_data[part_num] = chunk_data
                self.received_parts += 1
                
                # Progress update every 10 chunks or at completion
                if part_num % 10 == 0 or self.received_parts == self.total_parts:
                    progress = (self.received_parts / self.total_parts) * 100
                    elapsed = time.time() - self.start_time
                    print(f"📦 Progress: {self.received_parts}/{self.total_parts} chunks ({progress:.1f}%) - {elapsed:.1f}s elapsed")
            
            # Check if block is complete
            if self.received_parts == self.total_parts:
                self.reassemble_and_save()
                
        except Exception as e:
            print(f"❌ Error processing message: {e}")
            import traceback
            traceback.print_exc()
    
    def reassemble_and_save(self):
        """Reassemble all chunks and save to file"""
        try:
            elapsed = time.time() - self.start_time
            print(f"\n{'='*60}")
            print(f"✅ All chunks received! Reassembling...")
            print(f"   Total time: {elapsed:.2f}s")
            
            # Reassemble in correct order
            image_data = bytearray()
            for part_num in range(1, self.total_parts + 1):
                if part_num in self.parts_data:
                    image_data.extend(self.parts_data[part_num])
                else:
                    print(f"⚠️  Missing chunk {part_num}!")
                    return
            
            total_size = len(image_data)
            print(f"   Total size: {total_size:,} bytes ({total_size/1024:.2f} KB)")
            
            # Generate filename with timestamp
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"received_image_{timestamp}_block{self.block_id}.jpg"
            
            # Save to file
            with open(filename, 'wb') as f:
                f.write(image_data)
            
            print(f"💾 Saved to: {filename}")
            
            # Calculate throughput
            throughput = total_size / elapsed / 1024  # KB/s
            print(f"📊 Throughput: {throughput:.2f} KB/s")
            print(f"{'='*60}\n")
            
            # Reset for next block
            self.block_id = None
            self.parts_data = {}
            self.received_parts = 0
            
        except Exception as e:
            print(f"❌ Error reassembling image: {e}")
            import traceback
            traceback.print_exc()
    
    def start(self):
        """Start receiving images"""
        print("="*60)
        print("🖼️  Image Receiver - MQTT-SN Block Transfer")
        print("="*60)
        print(f"📡 Connecting to {self.broker_host}:{self.broker_port}...")
        
        try:
            self.client.connect(self.broker_host, self.broker_port, 60)
            print("\n⏳ Waiting for images from Pico W...")
            print("   (Press Ctrl+C to stop)\n")
            self.client.loop_forever()
            
        except KeyboardInterrupt:
            print("\n\n🛑 Stopping image receiver...")
        except Exception as e:
            print(f"❌ Error: {e}")
        finally:
            self.client.disconnect()
            print("👋 Disconnected")

if __name__ == "__main__":
    receiver = ImageReceiver()
    receiver.start()
