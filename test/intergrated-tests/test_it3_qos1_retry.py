#!/usr/bin/env python3
"""
IT3: QoS 1 - Sender resends message after timeout
MANUAL TEST: Requires observing serial output for DUP=1 flag
"""

print("="*60)
print("IT3: QoS 1 Retry with DUP Flag")
print("="*60)
print("\n⚠️  MANUAL TEST")
print("\nSteps:")
print("1. Set Pico to QoS 1 (press GP22)")
print("2. Stop MQTT-SN Gateway (Ctrl+C)")
print("3. Watch serial for retry with DUP=1")
print("4. Restart Gateway")
print("5. Verify PUBACK received and retries stop")
print("\nExpected:")
print("  - Sender retransmits with DUP=1 after timeout")
print("  - Receiver processes once and sends PUBACK")
print("  - Sender stops retries after PUBACK")
print("\nRecord results manually in Integration-Testing.txt")
