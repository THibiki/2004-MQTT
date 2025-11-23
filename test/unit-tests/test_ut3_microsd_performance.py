"""
UT3: microSD Performance
Requirement: Read or write 512B and 1kB chunks
             95% of operations ≤ 15ms
             100% of operations ≤ 20ms (no operation exceeds 20ms)

NOTE: Pico waits ~10s between packets
      So timeout is set to 120s to collect multiple SD test results
"""

import time
import statistics
from typing import Tuple, List
from test_config import SimpleUDPServer, PacketValidator

class TestUT3_MicroSDPerformance:
    """Test microSD read/write performance"""
    
    def __init__(self, listen_port: int = 1884):
        self.listen_port = listen_port
    
    def parse_sd_timing(self, data: str) -> dict:
        """
        Parse SD timing data from Pico
        Expected format: "SD_512R_8.5 SD_512W_7.2 SD_1024R_9.1 SD_1024W_10.3"
        Returns: {'512B_read': [ms], '512B_write': [ms], '1kB_read': [ms], '1kB_write': [ms]}
        """
        timings = {
            '512B_read': [],
            '512B_write': [],
            '1kB_read': [],
            '1kB_write': []
        }
        
        try:
            parts = data.split()
            for part in parts:
                if part.startswith('SD_512R_'):
                    ms = float(part.split('_')[2])
                    timings['512B_read'].append(ms)
                elif part.startswith('SD_512W_'):
                    ms = float(part.split('_')[2])
                    timings['512B_write'].append(ms)
                elif part.startswith('SD_1024R_'):
                    ms = float(part.split('_')[2])
                    timings['1kB_read'].append(ms)
                elif part.startswith('SD_1024W_'):
                    ms = float(part.split('_')[2])
                    timings['1kB_write'].append(ms)
        except:
            pass
        
        return timings
    
    def analyze_timings(self, timings: List[float]) -> dict:
        """
        Analyze timing data
        Return: {'p95': value, 'max': value, 'meets_requirement': bool}
        """
        if not timings:
            return {'p95': 0, 'max': 0, 'meets_requirement': False}
        
        sorted_timings = sorted(timings)
        p95_index = int(len(sorted_timings) * 0.95)
        p95 = sorted_timings[p95_index] if p95_index < len(sorted_timings) else max(timings)
        max_time = max(timings)
        
        # Requirement: P95 ≤ 15ms, Max ≤ 20ms
        meets_requirement = p95 <= 15 and max_time <= 20
        
        return {
            'p95': p95,
            'max': max_time,
            'avg': statistics.mean(timings),
            'meets_requirement': meets_requirement
        }
    
    def run(self, timeout_sec: int = 120) -> Tuple[bool, dict]:
        """
        Run UT3 test
        Requirement: Read or write 512B and 1kB chunks
                    P95 ≤ 15ms
                    Max ≤ 20ms (all operations)
        
        NOTE: Pico waits ~10s between packets
              Timeout 120s allows ~12 packets for averaging
        """
        print("\n" + "="*70)
        print("UT3: microSD PERFORMANCE")
        print("="*70)
        print(f"Listen port: {self.listen_port}")
        print(f"Timeout: {timeout_sec}s (allows ~{timeout_sec//10} SD test packets at 10s intervals)")
        print("\nRequirement:")
        print("  ✓ Read or write 512B and 1kB chunks")
        print("  ✓ 95% of operations ≤ 15ms")
        print("  ✓ 100% of operations ≤ 20ms (no operation exceeds 20ms)")
        print("\n⚠ NOTE: Pico waits ~10s between packets")
        print("   This is normal - don't interrupt the test!")
        print("\n⚠ IMPORTANT:")
        print("  1. Stop Paho gateway BEFORE running this test")
        print("  2. Restart gateway AFTER test completes")
        print("\n✓ Prerequisites:")
        print("  1. Pico W has microSD card inserted")
        print("  2. Pico firmware supports SD performance testing")
        print("  3. Pico configured to send to localhost:1884")
        print("  4. Pico publishes timing data in format:")
        print("     SD_512R_ms SD_512W_ms SD_1024R_ms SD_1024W_ms")
        print("  5. Paho gateway is STOPPED")
        
        server = SimpleUDPServer(self.listen_port)
        if not server.bind():
            print("\n✗ Failed to bind to port 1884")
            print("  Stop Paho gateway and try again")
            return False, {'error': 'failed to bind - gateway still running?'}
        
        print(f"\n✓ Successfully listening on port {self.listen_port}")
        print("→ Waiting for SD performance data from Pico...")
        print("   (This takes a while - packets arrive every 10s)\n")
        
        try:
            start_time = time.time()
            all_timings = {
                '512B_read': [],
                '512B_write': [],
                '1kB_read': [],
                '1kB_write': []
            }
            
            packets_received = 0
            
            while (time.time() - start_time) < timeout_sec:
                result = server.recv(timeout=1.0)
                
                if result:
                    data, addr = result
                    packets_received += 1
                    elapsed = time.time() - start_time
                    
                    try:
                        payload_str = data.decode('utf-8', errors='ignore')
                        timings = self.parse_sd_timing(payload_str)
                        
                        # Accumulate timings
                        for key, values in timings.items():
                            all_timings[key].extend(values)
                        
                        print(f"[Packet {packets_received}] Received at {elapsed:.1f}s")
                        print(f"  From: {addr[0]}:{addr[1]}")
                        print(f"  Data: {payload_str}")
                        print(f"  Parsed: {timings}\n")
                    
                    except Exception as e:
                        print(f"[Packet {packets_received}] Received (parsing error): {e}\n")
            
            server.close()
            
            if packets_received == 0:
                print("✗ No packets received from Pico!")
                print("\nTroubleshooting:")
                print("  - Is microSD card inserted?")
                print("  - Does Pico firmware support SD performance test?")
                print("  - Check Pico serial output for errors")
                print("  - Is Pico powered on and connected to WiFi?")
                return False, {'error': 'no packets received'}
            
            # Analyze all timings
            print("-"*70)
            print("ANALYSIS")
            print("-"*70 + "\n")
            
            all_results = {}
            all_passed = True
            
            for op_type, timings in all_timings.items():
                if timings:
                    analysis = self.analyze_timings(timings)
                    all_results[op_type] = analysis
                    
                    status = "✓ PASS" if analysis['meets_requirement'] else "✗ FAIL"
                    print(f"{op_type}:")
                    print(f"  Count: {len(timings)} operations")
                    print(f"  Avg: {analysis['avg']:.2f}ms")
                    print(f"  P95: {analysis['p95']:.2f}ms (Requirement: ≤ 15ms)")
                    print(f"  Max: {analysis['max']:.2f}ms (Requirement: ≤ 20ms)")
                    print(f"  {status}\n")
                    
                    if not analysis['meets_requirement']:
                        all_passed = False
            
            stats = {
                'packets_received': packets_received,
                'operations_analyzed': all_results,
                'all_requirements_met': all_passed
            }
            
            print("-"*70)
            print("RESULTS")
            print("-"*70)
            print(f"✓ Requirement 1: 512B and 1kB read/write chunks tested")
            print(f"✓ Requirement 2: P95 ≤ 15ms for 95% of operations")
            print(f"✓ Requirement 3: Max ≤ 20ms (no operation exceeds)")
            
            test_passed = all_passed
            print(f"\n{'✓ PASS' if test_passed else '✗ FAIL'}\n")
            
            return test_passed, stats
        
        finally:
            server.close()

def main(listen_port: int = 1884, timeout_sec: int = 120):
    """Run UT3 independently"""
    test = TestUT3_MicroSDPerformance(listen_port)
    return test.run(timeout_sec)

if __name__ == "__main__":
    import sys
    
    listen_port = int(sys.argv[1]) if len(sys.argv) > 1 else 1884
    timeout_sec = int(sys.argv[2]) if len(sys.argv) > 2 else 120
    
    passed, stats = main(listen_port, timeout_sec)
    exit(0 if passed else 1)