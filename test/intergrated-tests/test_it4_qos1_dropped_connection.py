#!/usr/bin/env python3
"""
IT4: QoS 1 - Dropped connection recovery
MANUAL TEST: Requires unplugging/replugging Pico
"""

print("="*60)
print("IT4: QoS 1 Dropped Connection")
print("="*60)
print("\n⚠️  MANUAL TEST")
print("\nSteps:")
print("1. Set Pico to QoS 1 (press GP22)")
print("2. Unplug Pico during message transmission")
print("3. Wait 3 seconds")
print("4. Plug Pico back in")
print("5. Observe reconnection and message retransmission")
print("\nExpected:")
print("  - After reconnect, sender resends with DUP=1")
print("  - Receiver sends PUBACK")
print("  - Final delivery exactly once")
print("\nRecord results manually in Integration-Testing.txt")
