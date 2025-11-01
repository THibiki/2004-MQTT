#!/usr/bin/env python3
"""
Test script for Pico W bidirectional MQTT communication
Publishes messages to topics that the Pico is subscribed to
"""

import paho.mqtt.client as mqtt
import time
import sys

BROKER_HOST = "localhost"
BROKER_PORT = 1883

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Connected to MQTT broker")
        # Subscribe to Pico's response topic
        client.subscribe("pico/response")
        print("📥 Subscribed to pico/response\n")
    else:
        print(f"❌ Connection failed with code {rc}")

def on_message(client, userdata, msg):
    print(f"\n📬 Received from Pico:")
    print(f"  Topic: {msg.topic}")
    print(f"  Payload: {msg.payload.decode('utf-8', errors='ignore')}")
    print()

def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        print("🔌 Connecting to MQTT broker...")
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        client.loop_start()
        time.sleep(1)
        
        print("\n" + "="*60)
        print("  Pico W Bidirectional Communication Test")
        print("="*60)
        print("\nCommands:")
        print("  1 - Send test message to pico/test (Pico echoes back)")
        print("  2 - Send 'ping' command to pico/command (Pico responds 'pong')")
        print("  3 - Send custom message to pico/test")
        print("  4 - Send custom message to pico/command")
        print("  q - Quit")
        print()
        
        while True:
            choice = input("Enter command: ").strip()
            
            if choice == 'q':
                break
            elif choice == '1':
                msg = "Hello Pico!"
                print(f"📤 Publishing to pico/test: '{msg}'")
                client.publish("pico/test", msg)
            elif choice == '2':
                msg = "ping"
                print(f"📤 Publishing to pico/command: '{msg}'")
                client.publish("pico/command", msg)
            elif choice == '3':
                msg = input("Enter message for pico/test: ")
                print(f"📤 Publishing to pico/test: '{msg}'")
                client.publish("pico/test", msg)
            elif choice == '4':
                msg = input("Enter message for pico/command: ")
                print(f"📤 Publishing to pico/command: '{msg}'")
                client.publish("pico/command", msg)
            else:
                print("Invalid choice")
            
            time.sleep(0.5)
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()
        print("\n👋 Disconnected")

if __name__ == "__main__":
    main()
