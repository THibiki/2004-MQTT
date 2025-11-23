#!/usr/bin/env python3
"""
IT12: SD card read/write operations
Tests SD card functionality
"""

print("="*60)
print("IT12: SD Card Operations")
print("="*60)
print("\n⚠️  MANUAL TEST")
print("\nSteps:")
print("1. Insert SD card with test image")
print("2. Press GP21 to trigger block transfer")
print("3. Watch serial monitor for SD card operations")
print("\nExpected serial output:")
print("  [SD] Initialising SD card...")
print("  [SD] SD card initalised and FAT32 mounted!")
print("  [APP] Scanning SD card for images...")
print("  [APP] Found image: filename.jpg")
print("  📁 Reading from SD card: filename.jpg")
print("  📊 File size: XXXXX bytes")
print("  ✅ Image loaded from SD card")
print("  📤 Sending to topic 'pico/chunks'")
print("\nSuccess criteria:")
print("  - Read and write test pattern matches original")
print("  - No bit errors")
print("  - LED blinks success pattern within 1 second")
print("\nAlternatively:")
print("  - Successful image read from SD")
print("  - Successful block transfer")
print("  - Image matches original after transfer")
print("\nRecord results manually in Integration-Testing.txt")
