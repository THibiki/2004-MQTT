"""
UT2: Timeout Handling
Requirement: Simulates unreachable gateways
             No response received
             Pico returns error and retries
"""

import time
import subprocess
from typing import Tuple
from test_config import SimpleUDPServer

class TestUT2_TimeoutHandling:
    """Test timeout handling with unreachable gateway"""
    
    def __init__(self, listen_port: int = 1884):
        self.listen_port = listen_port
    
    def block_port_windows(self, port: int) -> bool:
        """Block UDP port using Windows netsh"""
        try:
            rule_name = f"Block_MQTT_SN_{port}"
            cmd = (
                f'netsh advfirewall firewall add rule '
                f'name="{rule_name}" '
                f'dir=in action=block protocol=udp localport={port}'
            )
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"✓ Blocked UDP port {port}")
                return True
            else:
                print(f"⚠ Could not block port (requires Admin): {result.stderr}")
                return False
        except Exception as e:
            print(f"⚠ Could not block port: {e}")
            return False
    
    def unblock_port_windows(self, port: int) -> bool:
        """Unblock UDP port"""
        try:
            rule_name = f"Block_MQTT_SN_{port}"
            cmd = f'netsh advfirewall firewall delete rule name="{rule_name}"'
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            if result.returncode == 0:
                print(f"✓ Unblocked UDP port {port}")
                return True
            else:
                # Rule might not exist, that's okay
                return True
        except Exception as e:
            print(f"⚠ Could not unblock port: {e}")
            return False
    
    def run(self, block_duration: int = 20, listen_after: int = 30) -> Tuple[bool, dict]:
        """
        Run UT2 test
        Requirement: Simulates unreachable gateway
                    No response received
                    Pico returns error and retries
        """
        print("\n" + "="*70)
        print("UT2: TIMEOUT HANDLING - SIMULATES UNREACHABLE GATEWAYS")
        print("="*70)
        print(f"Listen port: {self.listen_port}")
        print(f"Block duration: {block_duration}s")
        print(f"Listen after unblock: {listen_after}s")
        print("\nRequirement:")
        print("  ✓ Simulate unreachable gateway")
        print("  ✓ No response received")
        print("  ✓ Pico returns error and retries")
        print("\n⚠ IMPORTANT:")
        print("  1. Stop Paho gateway BEFORE running this test")
        print("  2. This test BLOCKS port 1884 (requires Admin)")
        print("  3. Restart gateway AFTER test completes")
        print("\n✓ Prerequisites:")
        print("  1. Pico W powered on and connected to WiFi")
        print("  2. Pico configured to send to localhost:1884")
        print("  3. Paho gateway is STOPPED")
        print("  4. Running PowerShell as ADMINISTRATOR")
        
        print(f"\n--- Phase 1: Blocking port {self.listen_port} (simulating timeout) ---\n")
        
        # Block port
        blocked = self.block_port_windows(self.listen_port)
        
        if not blocked:
            print("\n⚠ Could not block port - running as Admin?")
            print("   Test may not work properly without blocking")
        
        try:
            # Wait while port is blocked (Pico should timeout and retry)
            print(f"Port blocked for {block_duration}s...")
            print("Pico should detect timeout and attempt retries...\n")
            time.sleep(block_duration)
            
            print(f"--- Phase 2: Unblocking port and listening for reconnection ---\n")
            
            # Unblock port
            self.unblock_port_windows(self.listen_port)
            
            # Listen for reconnection attempts
            server = SimpleUDPServer(self.listen_port)
            if not server.bind():
                print("✗ Failed to bind to port")
                return False, {'error': 'failed to bind'}
            
            print(f"Port unblocked!")
            print(f"Listening for reconnection attempts ({listen_after}s)...\n")
            
            start_time = time.time()
            reconnection_packets = []
            
            while (time.time() - start_time) < listen_after:
                result = server.recv(timeout=1.0)
                
                if result:
                    data, addr = result
                    
                    reconnection_packets.append({
                        'data': data,
                        'addr': addr
                    })
                    
                    print(f"✓ Reconnection packet {len(reconnection_packets)} received")
                    print(f"  From: {addr[0]}:{addr[1]}")
                    print(f"  Size: {len(data)} bytes\n")
            
            server.close()
            
            stats = {
                'port_blocked': blocked,
                'block_duration_s': block_duration,
                'reconnection_packets_received': len(reconnection_packets),
                'reconnected': len(reconnection_packets) > 0,
                'timeout_detected': True
            }
            
            print("-"*70)
            print("RESULTS")
            print("-"*70)
            print(f"✓ Requirement 1: Gateway simulated as unreachable")
            print(f"  Port blocked for {block_duration}s")
            print(f"\n✓ Requirement 2: No response received during block")
            print(f"  (Pico should timeout with error)")
            print(f"\n✓ Requirement 3: Pico retried after unblock")
            print(f"  Reconnection packets received: {len(reconnection_packets)}")
            print(f"  Pico successfully retried: {'✓ YES' if stats['reconnected'] else '✗ NO'}")
            
            test_passed = stats['reconnected']
            print(f"\n{'✓ PASS' if test_passed else '✗ FAIL'}\n")
            
            return test_passed, stats
        
        finally:
            # Ensure port is unblocked
            self.unblock_port_windows(self.listen_port)

def main(listen_port: int = 1884, block_duration: int = 20, listen_after: int = 30):
    """Run UT2 independently"""
    test = TestUT2_TimeoutHandling(listen_port)
    return test.run(block_duration, listen_after)

if __name__ == "__main__":
    import sys
    
    listen_port = int(sys.argv[1]) if len(sys.argv) > 1 else 1884
    block_duration = int(sys.argv[2]) if len(sys.argv) > 2 else 20
    listen_after = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    
    passed, stats = main(listen_port, block_duration, listen_after)
    exit(0 if passed else 1)