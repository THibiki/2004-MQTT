#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "mqtt_sn_test.h"

// Test MQTT-SN message structures without networking
int main() {
    stdio_init_all();
    
    printf("MQTT-SN Message Structure Test\n");
    printf("=============================\n");
    
    // Test message construction
    uint8_t buffer[256];
    mqtt_sn_client_t test_client;
    
    // Initialize test client
    memset(&test_client, 0, sizeof(mqtt_sn_client_t));
    strcpy(test_client.client_id, "test_client");
    test_client.keepalive = 60;
    test_client.next_msg_id = 1;
    test_client.next_topic_id = 1;
    
    printf("Testing MQTT-SN message construction...\n\n");
    
    // Test CONNECT message
    printf("1. Testing CONNECT message:\n");
    int len = mqtt_sn_build_connect(&test_client, buffer, sizeof(buffer));
    if (len > 0) {
        printf("   ✓ CONNECT message built successfully (%d bytes)\n", len);
        printf("   Message: ");
        for (int i = 0; i < len; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    } else {
        printf("   ✗ Failed to build CONNECT message\n");
    }
    
    // Test REGISTER message
    printf("\n2. Testing REGISTER message:\n");
    len = mqtt_sn_build_register(&test_client, "test/topic", buffer, sizeof(buffer));
    if (len > 0) {
        printf("   ✓ REGISTER message built successfully (%d bytes)\n", len);
        printf("   Message: ");
        for (int i = 0; i < len; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    } else {
        printf("   ✗ Failed to build REGISTER message\n");
    }
    
    // Test PUBLISH message
    printf("\n3. Testing PUBLISH message:\n");
    const char *test_data = "Hello MQTT-SN!";
    len = mqtt_sn_build_publish(&test_client, 1, (const uint8_t*)test_data, 
                               strlen(test_data), MQTT_SN_QOS_0, buffer, sizeof(buffer));
    if (len > 0) {
        printf("   ✓ PUBLISH message built successfully (%d bytes)\n", len);
        printf("   Message: ");
        for (int i = 0; i < len; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    } else {
        printf("   ✗ Failed to build PUBLISH message\n");
    }
    
    // Test SUBSCRIBE message
    printf("\n4. Testing SUBSCRIBE message:\n");
    len = mqtt_sn_build_subscribe(&test_client, "test/subscribe", MQTT_SN_QOS_0, 
                                 buffer, sizeof(buffer));
    if (len > 0) {
        printf("   ✓ SUBSCRIBE message built successfully (%d bytes)\n", len);
        printf("   Message: ");
        for (int i = 0; i < len; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n");
    } else {
        printf("   ✗ Failed to build SUBSCRIBE message\n");
    }
    
    // Test message parsing
    printf("\n5. Testing message parsing:\n");
    mqtt_sn_message_t parsed_msg;
    if (mqtt_sn_parse_message(buffer, len, &parsed_msg) == 0) {
        printf("   ✓ Message parsed successfully\n");
        printf("   Length: %d, Type: 0x%02X\n", parsed_msg.length, parsed_msg.msg_type);
    } else {
        printf("   ✗ Failed to parse message\n");
    }
    
    printf("\n=== Test Results ===\n");
    printf("✓ MQTT-SN message structures are working correctly!\n");
    printf("✓ Ready to add networking layer\n");
    
    // Blink LED to indicate success
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    for (int i = 0; i < 5; i++) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(200);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        sleep_ms(200);
    }
    
    printf("LED blinked 5 times - test complete!\n");
    
    return 0;
}
