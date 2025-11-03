#ifndef WIFI_DRIVER_H
#define WIFI_DRIVER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Error codes documented in Driver Interface 1
#define WIFI_OK           0  // Operation completed successfully
#define WIFI_EAUTH       -1  // WPA2 authentication failed (incorrect password)
#define WIFI_ETIMEDOUT   -2  // Operation timeout
#define WIFI_ENOTCONN    -3  // WiFi not connected when attempting UDP operation

// Initialize WiFi hardware
int wifi_init(void);

// Connect to WiFi network
int wifi_connect(const char *ssid, const char *password);

// Get assigned IP address (optional)
int wifi_get_ip(char *buf, size_t len);

// Create UDP socket and bind to local port
int wifi_udp_create(uint16_t local_port);

// Connect UDP socket to remote address (ensures consistent source port)
int wifi_udp_connect_remote(const char *dest_ip, uint16_t dest_port);

// Send UDP packet
int wifi_udp_send(const char *dest_ip, uint16_t dest_port, const uint8_t *data, size_t len);

// Receive UDP packet with timeout (ms). 0 = non-blocking
int wifi_udp_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms);

// Reset WiFi hardware
int wifi_reset(void);

// Check if WiFi is connected
bool wifi_is_connected(void);

// Get signal strength in dBm
int wifi_get_rssi(void);

#endif // WIFI_DRIVER_H