#!/usr/bin/env python3
"""
Simple Serial Monitor that auto-reconnects
Keeps monitoring even when Pico resets
"""

import serial
import time
import sys

# Configuration
BAUD_RATE = 115200
TIMEOUT = 1

def find_pico_port():
    """Try common COM ports"""
    import serial.tools.list_ports
    
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if 'USB Serial' in port.description or 'Pico' in port.description:
            return port.device
    
    # If not found, ask user
    print("Available COM ports:")
    for i, port in enumerate(ports):
        print(f"  {i+1}. {port.device} - {port.description}")
    
    if not ports:
        print("No COM ports found!")
        return None
    
    choice = input(f"\nEnter port number (1-{len(ports)}) or COM port name (e.g., COM3): ")
    
    if choice.upper().startswith('COM'):
        return choice.upper()
    else:
        try:
            idx = int(choice) - 1
            return ports[idx].device
        except:
            return None

def monitor_serial(port):
    """Monitor serial port with auto-reconnect"""
    print(f"="*60)
    print(f"Serial Monitor - {port} @ {BAUD_RATE} baud")
    print(f"="*60)
    print("Auto-reconnect enabled - will survive Pico resets")
    print("Press Ctrl+C to exit\n")
    
    ser = None
    
    try:
        while True:
            try:
                # Try to open/reopen port
                if ser is None or not ser.is_open:
                    try:
                        ser = serial.Serial(port, BAUD_RATE, timeout=TIMEOUT)
                        print(f"\n✅ Connected to {port}")
                    except serial.SerialException:
                        print(f"⚠️  Waiting for {port}...", end='\r')
                        time.sleep(1)
                        continue
                
                # Read data
                if ser.in_waiting > 0:
                    try:
                        data = ser.readline()
                        text = data.decode('utf-8', errors='ignore').rstrip()
                        if text:
                            print(text)
                    except:
                        pass  # Ignore decode errors
                
                time.sleep(0.01)  # Small delay to prevent CPU spinning
                
            except serial.SerialException:
                # Connection lost (Pico unplugged/reset)
                print("\n⚠️  Connection lost - waiting for reconnection...")
                if ser:
                    try:
                        ser.close()
                    except:
                        pass
                ser = None
                time.sleep(1)
                
    except KeyboardInterrupt:
        print("\n\n✅ Serial monitor stopped")
        if ser:
            ser.close()

if __name__ == "__main__":
    # Check if pyserial is installed
    try:
        import serial.tools.list_ports
    except ImportError:
        print("❌ pyserial not installed!")
        print("Install with: pip install pyserial")
        sys.exit(1)
    
    # Find port
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = find_pico_port()
    
    if not port:
        print("❌ No port specified or found")
        print("Usage: python serial_monitor.py COM3")
        sys.exit(1)
    
    # Start monitoring
    monitor_serial(port)
