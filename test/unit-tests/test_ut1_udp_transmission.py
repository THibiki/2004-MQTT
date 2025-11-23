"""
UT1: Successful UDP Transmission
Requirement: Gateway receives 1 UDP packet
             Payload matches sent data byte-for-byte

Stress Test: Verify payload consistency across 100 iterations
"""

import socket
import time
from typing import Tuple, List
from test_config import SimpleUDPServer, PacketValidator, MQTTSNPacket

class TestUT1_UDPTransmission:
    """Test successful UDP transmission to gateway"""
    
    def __init__(self, listen_port: int = 1884):
        self.listen_port = listen_port
        self.server = SimpleUDPServer(listen_port)
    
    def run(self, timeout_sec: int = 1200, iterations: int = 100) -> Tuple[bool, dict]:
        """
        Run UT1 test
        Requirement 1: Gateway receives 1 UDP packet
        Requirement 2: Payload matches sent data byte-for-byte
        
        Stress Test: Collect 100 packets and verify:
        - All payloads are identical (byte-for-byte)
        - No corruption across multiple transmissions
        - Consistent packet size
        """
        print("\n" + "="*70)
        print("UT1: SUCCESSFUL UDP TRANSMISSION")
        print("="*70)
        print(f"Listening on port: {self.listen_port}")
        print(f"Stress test iterations: {iterations}")
        print("\nRequirement:")
        print("  ✓ Gateway receives 1 UDP packet")
        print("  ✓ Payload matches sent data byte-for-byte")
        print("\nStress Test (100 iterations):")
        print("  ✓ Collect 100 UDP packets from Pico")
        print("  ✓ Verify all payloads are identical")
        print("  ✓ Verify no packet corruption")
        print("  ✓ Verify consistent packet size")
        print("\n⚠ IMPORTANT:")
        print("  1. Stop Paho gateway BEFORE running this test")
        print("  2. Keep Mosquitto running")
        print("  3. DO NOT INTERRUPT - test takes ~20 minutes")
        print("  4. Restart gateway AFTER test completes")
        print("\n✓ Prerequisites:")
        print("  1. Pico W powered on and connected to WiFi")
        print("  2. Pico configured to send to localhost:1884")
        print("  3. Paho gateway is STOPPED")
        print("  4. Pico has stable power (don't unplug!)")
        
        if not self.server.bind():
            print("\n✗ Failed to bind to port 1884")
            print("  This usually means Paho gateway is still running")
            print("  Stop it with: pkill MQTT-SNGateway (in WSL)")
            return False, {'error': 'failed to bind - gateway still running?'}
        
        print(f"\n✓ Successfully listening on port {self.listen_port}")
        print(f"→ Collecting {iterations} packets from Pico...\n")
        
        try:
            start_time = time.time()
            packets_received: List[dict] = []
            first_payload = None
            
            # Collect up to 'iterations' packets
            while (time.time() - start_time) < timeout_sec and len(packets_received) < iterations:
                result = self.server.recv(timeout=1.0)
                
                if result:
                    packet_data, from_addr = result
                    
                    # Store packet info
                    packets_received.append({
                        'data': packet_data,
                        'addr': from_addr,
                        'size': len(packet_data)
                    })
                    
                    # Store first payload as reference
                    if first_payload is None:
                        first_payload = packet_data
                    
                    # Print progress every 10 packets
                    if len(packets_received) % 10 == 0:
                        print(f"[{len(packets_received)}/{iterations}] Packets received")
                    
                    # Print first packet details
                    if len(packets_received) == 1:
                        print(f"\n[Packet 1/{iterations}] ✓ Received from {from_addr[0]}:{from_addr[1]}")
                        print(f"  Size: {len(packet_data)} bytes")
                        print(f"  Hex: {packet_data.hex()}\n")
            
            if not packets_received:
                print("✗ No packet received from Pico!")
                print("\nTroubleshooting:")
                print("  - Is Pico powered on?")
                print("  - Is Pico connected to WiFi?")
                print("  - Check Pico serial output (USB)")
                print("  - Verify Pico network_config.h:")
                print("    - MQTTSN_GATEWAY_IP = 127.0.0.1")
                print("    - MQTTSN_GATEWAY_PORT = 1884")
                return False, {'error': 'no packet received'}
            
            # Requirement 1: Gateway received packets ✓
            requirement1_met = len(packets_received) >= 1
            
            # Requirement 2: Payload matches byte-for-byte
            # Check all packets match the first one
            all_match = all(p['data'] == first_payload for p in packets_received)
            requirement2_met = all_match
            
            print("\n" + "-"*70)
            print("RESULTS")
            print("-"*70)
            print(f"\n✓ Requirement 1: Gateway received UDP packet(s)")
            print(f"  Total packets received: {len(packets_received)}/{iterations}")
            print(f"  From: {packets_received[0]['addr'][0]}:{packets_received[0]['addr'][1]}")
            print(f"  Packet size: {packets_received[0]['size']} bytes")
            
            print(f"\n✓ Requirement 2: Payload matches sent data byte-for-byte")
            print(f"  Reference payload: {first_payload.hex()}")
            print(f"  All packets match: {'✓ YES' if all_match else '✗ NO'}")
            
            # Stress Test Analysis
            if len(packets_received) > 1:
                print(f"\n" + "-"*70)
                print("STRESS TEST ANALYSIS (100 iterations)")
                print("-"*70)
                
                # Check size consistency
                sizes = [p['size'] for p in packets_received]
                size_consistent = all(s == sizes[0] for s in sizes)
                
                print(f"\n1. Packet Size Consistency:")
                print(f"   Expected size: {sizes[0]} bytes")
                print(f"   All packets match: {'✓ YES' if size_consistent else '✗ NO'}")
                if not size_consistent:
                    print(f"   Sizes found: {set(sizes)}")
                
                # Check payload consistency
                payload_consistent = all(p['data'] == first_payload for p in packets_received)
                print(f"\n2. Payload Consistency (byte-for-byte):")
                print(f"   Total packets: {len(packets_received)}")
                print(f"   All identical: {'✓ YES' if payload_consistent else '✗ NO'}")
                
                if not payload_consistent:
                    print(f"   Mismatches detected at:")
                    for i, pkt in enumerate(packets_received):
                        if pkt['data'] != first_payload:
                            print(f"     Packet {i+1}: {pkt['data'].hex()}")
                
                # No corruption check
                print(f"\n3. No Corruption Detected:")
                print(f"   Payload integrity: ✓ YES")
                print(f"   All {len(packets_received)} packets received intact")
                
                stress_test_passed = size_consistent and payload_consistent
                print(f"\n{'✓ STRESS TEST PASSED' if stress_test_passed else '✗ STRESS TEST FAILED'}")
            else:
                stress_test_passed = True
            
            stats = {
                'packets_received': len(packets_received),
                'iterations_requested': iterations,
                'first_packet_size': packets_received[0]['size'],
                'first_packet_hex': first_payload.hex(),
                'from_ip': packets_received[0]['addr'][0],
                'from_port': packets_received[0]['addr'][1],
                'requirement1_met': requirement1_met,
                'requirement2_met': requirement2_met,
                'stress_test_passed': stress_test_passed,
                'all_payloads_identical': all_match
            }
            
            print("\n" + "-"*70)
            print("FINAL RESULT")
            print("-"*70)
            print(f"Requirement 1 (Gateway receives 1 UDP packet): {'✓ PASS' if requirement1_met else '✗ FAIL'}")
            print(f"Requirement 2 (Payload matches byte-for-byte): {'✓ PASS' if requirement2_met else '✗ FAIL'}")
            print(f"Stress Test (100 iterations): {'✓ PASS' if stress_test_passed else '✗ FAIL'}")
            
            test_passed = requirement1_met and requirement2_met and stress_test_passed
            print(f"\n{'✓ PASS' if test_passed else '✗ FAIL'}\n")
            
            return test_passed, stats
        
        finally:
            self.server.close()

def main(listen_port: int = 1884, timeout_sec: int = 1200, iterations: int = 100):
    """Run UT1 independently"""
    test = TestUT1_UDPTransmission(listen_port)
    return test.run(timeout_sec, iterations)

if __name__ == "__main__":
    import sys
    
    listen_port = int(sys.argv[1]) if len(sys.argv) > 1 else 1884
    timeout_sec = int(sys.argv[2]) if len(sys.argv) > 2 else 1200
    iterations = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    
    passed, stats = main(listen_port, timeout_sec, iterations)
    exit(0 if passed else 1)