// wifi.h - Simple WiFi reconnection without scanning

#ifndef WIFI_H
#define WIFI_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <string.h>

// Configuration
#define RECONNECT_CHECK_INTERVAL_MS 5000   // Check connection every 5 seconds
#define RECONNECT_ATTEMPT_INTERVAL_MS 10000 // Try reconnecting every 10 seconds
#define CONNECTION_TIMEOUT_MS 20000         // Wait 20s for connection attempt

// Simple WiFi state
typedef struct {
    char ssid[33];
    char password[64];
    bool connected;
    absolute_time_t last_check_time;
    absolute_time_t last_reconnect_time;
    uint32_t reconnect_count;
    uint32_t disconnect_count;
} simple_wifi_t;

// Function Declaration 
int wifi_init(const char *ssid, const char *password);
bool wifi_is_connected(void);
const char* wifi_get_status(void);
int wifi_connect(void);
void wifi_auto_reconnect(void);
void wifi_print_stats(void);
void wifi_disconnect(void);

#endif // WIFI_H