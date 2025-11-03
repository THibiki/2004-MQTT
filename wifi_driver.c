#include "wifi_driver.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <string.h>
#include <stdio.h>
#include <lwip/udp.h>
#include <lwip/netif.h>

static bool initialized = false;
static bool connected = false;
static struct udp_pcb *udp_pcb = NULL;
static uint8_t *recv_buffer = NULL;
static size_t recv_len = 0;
static bool data_received = false;

// Persistent buffer to cache incoming packets
#define PACKET_CACHE_SIZE 1024
static uint8_t packet_cache[PACKET_CACHE_SIZE];
static size_t packet_cache_len = 0;
static bool packet_cached = false;

// Callback for UDP receive
static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                               const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        if (recv_buffer != NULL) {
            // Direct copy to application buffer
            size_t copy_len = p->len < recv_len ? p->len : recv_len;
            memcpy(recv_buffer, p->payload, copy_len);
            recv_len = copy_len;
            data_received = true;
        } else {
            // Cache the packet for later retrieval
            size_t copy_len = p->len < PACKET_CACHE_SIZE ? p->len : PACKET_CACHE_SIZE;
            memcpy(packet_cache, p->payload, copy_len);
            packet_cache_len = copy_len;
            packet_cached = true;
        }
        pbuf_free(p);
    }
}

int wifi_init(void) {
    if (initialized) {
        return WIFI_OK;
    }

    if (cyw43_arch_init()) {
        printf("CYW43 init failed\n");
        // Use WIFI_ETIMEDOUT for an unrecoverable hardware failure if a more specific error is not available.
        return WIFI_ETIMEDOUT;
    }

    cyw43_arch_enable_sta_mode();
    initialized = true;
    printf("WiFi hardware initialized\n");
    return WIFI_OK;
}

int wifi_connect(const char *ssid, const char *password) {
    if (!initialized) { 
        return WIFI_ENOTCONN;
    }

    if (ssid == NULL || password == NULL) {
        return WIFI_ETIMEDOUT;
    }

    // Try up to 3 times
    for (int attempt = 1; attempt <= 3; attempt++) {
        printf("Connection attempt %d/3 to: %s\n", attempt, ssid);
        
        // Deinit and restart WiFi state on retries
        if (attempt > 1) {
            cyw43_arch_deinit();
            sleep_ms(1000);
            cyw43_arch_init_with_country(CYW43_COUNTRY_USA);
            cyw43_arch_enable_sta_mode();
            cyw43_wifi_pm(&cyw43_state, 0xa11140);
            sleep_ms(2000);
        }
        
        int result = cyw43_arch_wifi_connect_timeout_ms(
            ssid, 
            password, 
            CYW43_AUTH_WPA2_AES_PSK, 
            30000  // 30 second timeout
        );

        if (result == 0) {
            connected = true;
            printf("WiFi connected successfully on attempt %d\n", attempt);
            return WIFI_OK;
        }
        
        printf("Attempt %d failed (error %d)\n", attempt, result);
        sleep_ms(2000);
    }
    
    printf("WiFi connection failed after 3 attempts\n");
    return WIFI_ETIMEDOUT;
}

int wifi_get_ip(char *buf, size_t len) {
    // Treat all failures here as not connected
    if (!initialized || !connected || buf == NULL || len == 0) {
        return WIFI_ENOTCONN;
    }

    struct netif *netif = netif_default;
    if (netif == NULL) {
        return WIFI_ENOTCONN;
    }

    snprintf(buf, len, "%s", ip4addr_ntoa(netif_ip4_addr(netif)));
    return WIFI_OK;
}

int wifi_udp_create(uint16_t local_port) {
    // Treat all failures here as not connected
    if (!initialized || !connected) {
        return WIFI_ENOTCONN;
    }

    // Close existing PCB if open
    if (udp_pcb != NULL) {
        udp_remove(udp_pcb);
    }

    udp_pcb = udp_new();
    if (udp_pcb == NULL) {
        printf("Failed to create UDP PCB\n");
        return WIFI_ENOTCONN;
    }

    // Binding failure (e.g., port in use) treated as not connected.
    err_t err = udp_bind(udp_pcb, IP_ADDR_ANY, local_port);
    if (err != ERR_OK) {
        printf("Failed to bind UDP PCB to port %d\n", local_port);
        udp_remove(udp_pcb);
        udp_pcb = NULL;
        return WIFI_ENOTCONN;
    }

    // Set up receive callback
    udp_recv(udp_pcb, udp_recv_callback, NULL);

    printf("UDP socket created on port %d\n", local_port);
    return WIFI_OK;
}

int wifi_udp_connect_remote(const char *dest_ip, uint16_t dest_port) {
    if (!initialized || !connected || udp_pcb == NULL) {
        return WIFI_ENOTCONN;
    }

    if (dest_ip == NULL || dest_port == 0) {
        return WIFI_ETIMEDOUT;
    }

    ip_addr_t dest_addr;
    if (!ip4addr_aton(dest_ip, &dest_addr)) {
        return WIFI_ETIMEDOUT;
    }

    // Connect PCB to remote address - this locks the source port
    err_t err = udp_connect(udp_pcb, &dest_addr, dest_port);
    if (err != ERR_OK) {
        printf("Failed to connect UDP PCB to %s:%d (err=%d)\n", dest_ip, dest_port, err);
        return WIFI_ETIMEDOUT;
    }

    printf("UDP PCB connected to %s:%d (source port locked to %d)\n", 
           dest_ip, dest_port, udp_pcb->local_port);
    return WIFI_OK;
}

int wifi_udp_send(const char *dest_ip, uint16_t dest_port, const uint8_t *data, size_t len) {
    // Treat all failures here as not connected
    if (!initialized || !connected || udp_pcb == NULL) {
        return WIFI_ENOTCONN;
    }

    // Treat invalid args as timeout (simplifying error codes)
    if (data == NULL) {
        return WIFI_ETIMEDOUT;
    }

    // Try to allocate pbuf
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL) {
        // Treat memory failure as timeout
        return WIFI_ETIMEDOUT;
    }

    memcpy(p->payload, data, len);

    // Always use udp_send() since PCB is connected
    err_t err = udp_send(udp_pcb, p);
    pbuf_free(p);

    if (err != ERR_OK) {
        printf("[ERROR] udp_send failed: %d\n", err);
        return WIFI_ETIMEDOUT;
    }

    return WIFI_OK;  // Success - return 0 instead of length
}

int wifi_udp_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    // Treat all failures here as not connected
    if (!initialized || !connected || udp_pcb == NULL || buffer == NULL) {
        return WIFI_ENOTCONN;
    }

    // Check if we have a cached packet first
    if (packet_cached) {
        size_t copy_len = packet_cache_len < max_len ? packet_cache_len : max_len;
        memcpy(buffer, packet_cache, copy_len);
        packet_cached = false;
        printf("wifi_udp_receive() returning: %d bytes (from cache)\n", copy_len);
        return copy_len;
    }

    // Set up receive buffer
    recv_buffer = buffer;
    recv_len = max_len;
    data_received = false;

    if (timeout_ms == 0) {
        // Non-blocking mode: Poll once and return immediately
        cyw43_arch_poll();
        if (!data_received) {
            recv_buffer = NULL;
            // Return 0 bytes received for non-blocking poll with no data
            return 0; 
        }
    } else {
        // Blocking mode with timeout
        absolute_time_t timeout_time = make_timeout_time_ms(timeout_ms);
        
        while (!data_received) {
            // Process any pending packets
            cyw43_arch_poll();
            
            if (time_reached(timeout_time)) {
                recv_buffer = NULL;
                return WIFI_ETIMEDOUT; // Operation timeout
            }
            
            // Sleep for 1ms to yield CPU and avoid spinning too fast
            sleep_ms(1);
        }
    }

    int bytes_received = recv_len;
    recv_buffer = NULL;
    data_received = false;
    
    return bytes_received;
}

int wifi_reset(void) {
    if (udp_pcb != NULL) {
        udp_remove(udp_pcb);
        udp_pcb = NULL;
    }

    connected = false;

    if (initialized) {
        cyw43_arch_deinit();
        initialized = false;
    }

    // Re-initialize
    return wifi_init();
}

bool wifi_is_connected(void) {
    return connected && initialized;
}

int wifi_get_rssi(void) {
    // Treat all failures here as not connected
    if (!initialized || !connected) {
        return WIFI_ENOTCONN;
    }

    int32_t rssi;
    // cyw43_wifi_get_rssi returns 0 on success. Treat failure as not connected.
    if (cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0) {
        return rssi;
    }
    
    return WIFI_ENOTCONN;
}