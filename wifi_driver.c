#include "wifi_driver.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"
#include <stdio.h>
#include <string.h>

static struct udp_pcb *udp_pcb = NULL;
static ip_addr_t target_addr;
static uint16_t target_port;
static uint8_t recv_buffer[1024];
static size_t recv_len = 0;
static bool recv_ready = false;

// UDP receive callback
static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        size_t len = p->len;
        if (len <= sizeof(recv_buffer)) {
            memcpy(recv_buffer, p->payload, len);
            recv_len = len;
            recv_ready = true;
            printf("UDP received %zu bytes from %s:%d\n", len, 
                   ip4addr_ntoa(ip_2_ip4(addr)), port);
        }
        pbuf_free(p);
    }
}

int wifi_init(void) {
    if (cyw43_arch_init()) {
        printf("Failed to initialize WiFi\n");
        return -1;
    }
    
    cyw43_arch_enable_sta_mode();
    printf("WiFi initialized\n");
    return WIFI_OK;
}

int wifi_connect(const char *ssid, const char *password) {
    printf("Connecting to WiFi network: %s\n", ssid);
    
    // Retry logic - attempt up to 3 times
    int max_retries = 3;
    int attempt = 0;
    
    for (attempt = 1; attempt <= max_retries; attempt++) {
        printf("\n[Attempt %d/%d] Connecting to %s...\n", attempt, max_retries, ssid);
        
        // Try to connect with 15-second timeout per attempt
        if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 15000) == 0) {
            printf("✅ WiFi authentication successful\n");
            break;
        } else {
            printf("❌ Connection attempt %d failed\n", attempt);
            
            // Don't retry if this was the last attempt
            if (attempt < max_retries) {
                printf("   Retrying in 2 seconds...\n");
                sleep_ms(2000);
            } else {
                printf("❌ All %d connection attempts failed\n", max_retries);
                return WIFI_EAUTH;
            }
        }
    }
    
    printf("✅ WiFi connected successfully\n");
    
    // Wait for IP assignment and link establishment
    int retries = 30;
    printf("Waiting for link establishment");
    while (retries-- > 0) {
        if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
            printf(" - Link UP!\n");
            break;
        }
        printf(".");
        sleep_ms(1000);
    }
    
    if (retries <= 0) {
        printf("\n⚠️  WiFi link not established within timeout\n");
        printf("Proceeding anyway - connection may still be functional\n");
    }
    
    // Print IP address if available
    extern struct netif *netif_default;
    if (netif_default && netif_is_up(netif_default)) {
        printf("IP address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    } else {
        printf("IP address not yet assigned\n");
    }
    
    return WIFI_OK;
}

int wifi_disconnect(void) {
    // Note: There's no explicit disconnect function in the current Pico SDK
    // The connection will be cleaned up when the device resets
    printf("WiFi disconnect requested\n");
    return WIFI_OK;
}

bool wifi_is_connected(void) {
    return cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP;
}

int wifi_udp_create(uint16_t local_port) {
    udp_pcb = udp_new();
    if (udp_pcb == NULL) {
        printf("Failed to create UDP PCB\n");
        return -1;
    }
    
    err_t err = udp_bind(udp_pcb, IP_ADDR_ANY, local_port);
    if (err != ERR_OK) {
        printf("Failed to bind UDP socket to port %d: %d\n", local_port, err);
        udp_remove(udp_pcb);
        udp_pcb = NULL;
        return -1;
    }
    
    udp_recv(udp_pcb, udp_recv_callback, NULL);
    
    printf("UDP socket created and bound to port %d\n", local_port);
    return WIFI_OK;
}

int wifi_udp_send(const char *host, uint16_t port, const uint8_t *data, size_t len) {
    if (udp_pcb == NULL) {
        printf("UDP not initialized\n");
        return WIFI_ENOTCONN;
    }
    
    // Resolve hostname to IP
    ip_addr_t server_ip;
    err_t err = dns_gethostbyname(host, &server_ip, NULL, NULL);
    if (err != ERR_OK) {
        // Try to parse as IP address
        if (!ip4addr_aton(host, ip_2_ip4(&server_ip))) {
            printf("Failed to resolve host: %s\n", host);
            return -1;
        }
    }
    
    // Create pbuf and send
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL) {
        printf("Failed to allocate pbuf\n");
        return -1;
    }
    
    memcpy(p->payload, data, len);
    
    err = udp_sendto(udp_pcb, p, &server_ip, port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("UDP send failed: %d\n", err);
        return -1;
    }
    
    return WIFI_OK;
}

int wifi_udp_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    if (udp_pcb == NULL) {
        printf("UDP not initialized\n");
        return WIFI_ENOTCONN;
    }
    
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    recv_ready = false;
    
    // Non-blocking check first
    if (timeout_ms == 0) {
        cyw43_arch_poll();
        if (!recv_ready) {
            return WIFI_ETIMEDOUT;
        }
    } else {
        // Blocking with timeout
        while (!recv_ready) {
            cyw43_arch_poll();
            sleep_ms(1);
            
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
            if (elapsed >= timeout_ms) {
                return WIFI_ETIMEDOUT;
            }
        }
    }
    
    if (recv_len > max_len) {
        printf("Received data too large (%zu > %zu)\n", recv_len, max_len);
        recv_ready = false;
        return -1;
    }
    
    memcpy(buffer, recv_buffer, recv_len);
    size_t len = recv_len;
    recv_ready = false;
    
    return (int)len;
}