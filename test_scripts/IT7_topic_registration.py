#!/usr/bin/env python3
"""
IT7: Successful topic registration
Tests REGISTER -> REGACK flow
"""

print("="*60)
print("IT7: Topic Registration")
print("="*60)
print("\n⚠️  OBSERVATION TEST")
print("\nSteps:")
print("1. Reset Pico W (unplug/replug)")
print("2. Watch serial monitor during startup")
print("3. Look for REGISTER and REGACK messages")
print("\nExpected output in serial:")
print("  [MQTTSN] Registering topic 'pico/test'...")
print("  [DEBUG] REGISTER packet: ...")
print("  [MQTTSN] ✓ Topic registered (TopicID=X, MsgID=Y)")
print("  [MQTTSN] Registering topic 'pico/chunks'...")
print("  [MQTTSN] ✓ Topic 'pico/chunks' registered (TopicID=Z)")
print("\nSuccess criteria:")
print("  - Client receives REGACK with non-zero topic ID")
print("  - Response within 800ms")
print("\nRecord results manually in Integration-Testing.txt")
