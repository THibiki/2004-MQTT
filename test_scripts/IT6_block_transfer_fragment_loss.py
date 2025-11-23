#!/usr/bin/env python3
"""
IT6: Block transfer with fragment loss
MANUAL TEST: Requires observing retry behavior
"""

print("="*60)
print("IT6: Block Transfer with Fragment Loss")
print("="*60)
print("\n⚠️  MANUAL TEST")
print("\nSteps:")
print("1. Start block transfer (press GP21)")
print("2. During transfer, briefly disconnect network/gateway")
print("3. Observe missing fragment detection")
print("4. Verify sender retransmits missing fragment")
print("5. Verify reassembly completes successfully")
print("\nExpected:")
print("  - Missing fragment detected")
print("  - Sender retransmits only missing fragment")
print("  - Reassembly completes successfully")
print("  - Final payload matches reference checksum")
print("\nNote: With QoS 1/2, retransmission is automatic")
print("Record results manually in Integration-Testing.txt")
