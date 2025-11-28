#!/usr/bin/env python3
"""
Master Test Runner for Automated Integration Tests
Runs all automated and semi-automated integration tests in sequence
"""

import subprocess
import sys
import time
from pathlib import Path

# Test configuration
TEST_DIR = Path(__file__).parent

# Test definitions: (filename, test_id, automation_level, description)
TESTS = [
    # Fully automated tests (no manual intervention during run)
    ("test_it1_qos0_message_delivery.py", "IT1", "auto", "QoS 0 Message Delivery"),
    ("test_it2_qos1_message_delivery.py", "IT2", "auto", "QoS 1 Message Delivery with PUBACK"),
    ("test_it8_topic_subscription.py", "IT8", "auto", "Topic Subscription"),
    
    # Semi-automated tests (require manual intervention but scripted checks)
    ("test_it3_qos1_retry_automated.py", "IT3", "semi", "QoS 1 Retry with Gateway Control"),
    ("test_it4_qos1_dropped_connection_automated.py", "IT4", "semi", "QoS 1 Dropped Connection"),
    ("test_it6_block_transfer_fragment_loss_automated.py", "IT6", "semi", "Block Transfer Fragment Loss"),
    ("test_it7_topic_registration_automated.py", "IT7", "semi", "Topic Registration"),
    ("test_it9_wifi_auto_reconnect_automated.py", "IT9", "semi", "WiFi Auto-Reconnect"),
    ("test_it10_topic_registration_after_disconnect_automated.py", "IT10", "semi", "Topic Registration After Disconnect"),
    ("test_it11_wifi_connection_automated.py", "IT11", "semi", "WiFi Connection on Boot"),
    ("test_it12_sd_card_operations_automated.py", "IT12", "semi", "SD Card Operations"),
]

def print_header(text):
    """Print formatted header"""
    print("\n" + "="*70)
    print(text.center(70))
    print("="*70 + "\n")

def print_test_info(test_id, description, automation_level):
    """Print test information"""
    level_text = "AUTOMATED" if automation_level == "auto" else "SEMI-AUTOMATED"
    print(f"\n{'─'*70}")
    print(f"{test_id}: {description} ({level_text})")
    print(f"{'─'*70}")

def run_test(test_file, test_id, automation_level, description):
    """Run a single test"""
    print_test_info(test_id, description, automation_level)
    
    test_path = TEST_DIR / test_file
    
    if not test_path.exists():
        print(f"❌ Test file not found: {test_file}")
        return False
    
    try:
        # Run test
        result = subprocess.run(
            [sys.executable, str(test_path)],
            cwd=str(TEST_DIR),
            capture_output=False,  # Show output in real-time
            text=True
        )
        
        if result.returncode == 0:
            print(f"\n✅ {test_id} completed successfully")
            return True
        else:
            print(f"\n⚠️  {test_id} exited with code {result.returncode}")
            return False
            
    except KeyboardInterrupt:
        print(f"\n\n⚠️  {test_id} interrupted by user")
        raise
    except Exception as e:
        print(f"\n❌ Error running {test_id}: {e}")
        return False

def run_all_tests(test_type="all"):
    """Run all tests of specified type"""
    print_header("MQTT-SN Integration Test Suite")
    
    print("Available test categories:")
    print("  1. all   - Run all tests (automated + semi-automated)")
    print("  2. auto  - Run only fully automated tests")
    print("  3. semi  - Run only semi-automated tests")
    print()
    
    # Filter tests based on type
    if test_type == "auto":
        tests_to_run = [t for t in TESTS if t[2] == "auto"]
        print("Running AUTOMATED tests only\n")
    elif test_type == "semi":
        tests_to_run = [t for t in TESTS if t[2] == "semi"]
        print("Running SEMI-AUTOMATED tests only\n")
    else:
        tests_to_run = TESTS
        print("Running ALL tests\n")
    
    print(f"Total tests to run: {len(tests_to_run)}\n")
    
    print("⚠️  IMPORTANT PREREQUISITES:")
    print("  1. Mosquitto broker running in WSL")
    print("  2. MQTT-SN Gateway running in WSL (for most tests)")
    print("  3. Pico W connected and running MQTT-SN client")
    print("  4. Serial monitor available for verification")
    print("  5. Python dependencies installed: paho-mqtt")
    print()
    
    response = input("Are all prerequisites met? (y/n): ")
    if response.lower() != 'y':
        print("\n⚠️  Please ensure all prerequisites are met before running tests")
        return
    
    # Run tests
    results = []
    start_time = time.time()
    
    for i, (test_file, test_id, automation_level, description) in enumerate(tests_to_run, 1):
        print(f"\n\n{'█'*70}")
        print(f"Test {i}/{len(tests_to_run)}")
        print(f"{'█'*70}")
        
        success = run_test(test_file, test_id, automation_level, description)
        results.append((test_id, description, success))
        
        # Delay between tests
        if i < len(tests_to_run):
            print(f"\n⏳ Waiting 5 seconds before next test...")
            time.sleep(5)
    
    # Print summary
    elapsed = time.time() - start_time
    print_header("TEST SUITE SUMMARY")
    
    passed = sum(1 for _, _, success in results if success)
    failed = len(results) - passed
    
    print(f"Total Tests Run: {len(results)}")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Success Rate: {(passed/len(results)*100):.1f}%")
    print(f"Total Time: {elapsed/60:.1f} minutes")
    print()
    
    # Detailed results
    print("Detailed Results:")
    print(f"{'─'*70}")
    for test_id, description, success in results:
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"{test_id:5} | {status} | {description}")
    print(f"{'─'*70}")
    
    # Next steps
    print("\n📝 Next Steps:")
    print("  1. Review serial monitor output for detailed verification")
    print("  2. Update test2.txt with tolerance results from each test")
    print("  3. For failed tests, check:")
    print("     - Pico W connection and status")
    print("     - MQTT-SN Gateway status")
    print("     - Network connectivity")
    print("     - Serial output for error messages")

def main():
    """Main entry point"""
    if len(sys.argv) > 1:
        test_type = sys.argv[1].lower()
        if test_type not in ["all", "auto", "semi"]:
            print("Usage: python run_automated_tests.py [all|auto|semi]")
            print("  all  - Run all tests (default)")
            print("  auto - Run only automated tests")
            print("  semi - Run only semi-automated tests")
            sys.exit(1)
    else:
        test_type = "all"
    
    try:
        run_all_tests(test_type)
    except KeyboardInterrupt:
        print("\n\n⚠️  Test suite interrupted by user")
        sys.exit(1)

if __name__ == "__main__":
    main()
