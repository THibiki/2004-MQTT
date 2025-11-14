#!/usr/bin/env python3
"""
Test Script for Pico-to-Pico Communication
This simulates messages being published to test the Pico receiver
"""

import paho.mqtt.client as mqtt
import time
import sys

BROKER = "localhost"
PORT = 1883

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to broker at {BROKER}:{PORT}")
        client.subscribe("pico/#")
        print("📡 Subscribed to pico/# (monitoring all pico topics)")
    else:
        print(f"❌ Connection failed: {rc}")
        sys.exit(1)

def on_message(client, userdata, msg):
    print(f"\n📬 [{msg.topic}] ", end="")
    try:
        # Try to decode as text
        text = msg.payload.decode('utf-8', errors='ignore')
        if len(text) < 100 and all(c.isprintable() or c in '\n\r\t' for c in text):
            print(f"{text}")
        else:
            print(f"{len(msg.payload)} bytes (binary data)")
    except:
        print(f"{len(msg.payload)} bytes (binary)")

def main():
    print("\n=== Pico-to-Pico Communication Test ===\n")
    
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(BROKER, PORT, 60)
        client.loop_start()
        
        print("\n📤 Sending test messages to Picos...\n")
        time.sleep(1)
        
        # Test 1: Send command
        print("Test 1: Sending command to pico/commands")
        client.publish("pico/commands", "TEST_COMMAND_FROM_PC")
        time.sleep(2)
        
        # Test 2: Send text message
        print("Test 2: Sending text message to pico/commands")
        client.publish("pico/commands", "Hello Pico! This is a test message.")
        time.sleep(2)
        
        # Test 3: Monitor responses
        print("\nTest 3: Monitoring for Pico responses...")
        print("(Press Ctrl+C to exit)")
        print("-" * 50)
        
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\n\n✅ Test complete - exiting")
    except Exception as e:
        print(f"\n❌ Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
