#!/usr/bin/env python3
"""
IT10: Topic registration after disconnect
Tests re-registration after reconnection
"""

print("="*60)
print("IT10: Topic Registration After Disconnect")
print("="*60)
print("\n⚠️  MANUAL TEST")
print("\nSteps:")
print("1. Pico connected and MQTT-SN initialized")
print("2. Stop MQTT-SN Gateway")
print("3. Watch serial for disconnect detection")
print("4. Restart Gateway")
print("5. Observe reconnection and re-registration")
print("\nExpected serial output:")
print("  [MQTTSN] Connection lost - will reconnect...")
print("  [TEST] Initializing MQTT-SN client...")
print("  [MQTTSN] Registering topic 'pico/test'...")
print("  [MQTTSN] ✓ Topic registered (TopicID=X)")
print("  [MQTTSN] Registering topic 'pico/chunks'...")
print("  [MQTTSN] ✓ Topic 'pico/chunks' registered")
print("\nSuccess criteria:")
print("  - REGISTER request succeeds")
print("  - REGACK received within 800ms")
print("  - Assigned topic ID stored and usable")
print("\nRecord results manually in Integration-Testing.txt")
