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
        return WIFI_ERROR;
    }
    
    cyw43_arch_enable_sta_mode();
    printf("WiFi initialized\n");
    return WIFI_OK;
}

int wifi_connect(const char *ssid, const char *password) {
    printf("Connecting to WiFi network: %s\n", ssid);
    
    if (cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Failed to connect to WiFi\n");
        return WIFI_ERROR;
    }
    
    printf("WiFi connected successfully\n");
    
    // Wait for IP assignment and link establishment
    int retries = 30; // Increased retries
    printf("Waiting for link establishment");
    while (retries-- > 0) {
        if (cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
            printf(" - Link UP!\n");
            break;
        }
        printf(".");
        sleep_ms(1000); // Wait 1 second between checks
    }
    
    if (retries <= 0) {
        printf("\nWiFi link not established within timeout\n");
        // Don't return error - connection might still work
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
        return WIFI_ERROR;
    }
    
    err_t err = udp_bind(udp_pcb, IP_ADDR_ANY, local_port);
    if (err != ERR_OK) {
        printf("Failed to bind UDP socket to port %d: %d\n", local_port, err);
        udp_remove(udp_pcb);
        udp_pcb = NULL;
        return WIFI_ERROR;
    }
    
    udp_recv(udp_pcb, udp_recv_callback, NULL);
    
    printf("UDP socket created and bound to port %d\n", local_port);
    return WIFI_OK;
}

int wifi_udp_send(const char *host, uint16_t port, const uint8_t *data, size_t len) {
    if (udp_pcb == NULL) {
        printf("UDP not initialized\n");
        return WIFI_ERROR;
    }
    
    // Resolve hostname to IP
    ip_addr_t server_ip;
    err_t err = dns_gethostbyname(host, &server_ip, NULL, NULL);
    if (err != ERR_OK) {
        // Try to parse as IP address
        if (!ip4addr_aton(host, ip_2_ip4(&server_ip))) {
            printf("Failed to resolve host: %s\n", host);
            return WIFI_ERROR;
        }
    }
    
    // Create pbuf and send
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL) {
        printf("Failed to allocate pbuf\n");
        return WIFI_ERROR;
    }
    
    memcpy(p->payload, data, len);
    
    err = udp_sendto(udp_pcb, p, &server_ip, port);
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("UDP send failed: %d\n", err);
        return WIFI_ERROR;
    }
    
    return WIFI_OK;
}

int wifi_udp_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    if (udp_pcb == NULL) {
        printf("UDP not initialized\n");
        return WIFI_ERROR;
    }
    
    uint32_t start_time = to_ms_since_boot(get_absolute_time());
    recv_ready = false;
    
    // Non-blocking check first
    if (timeout_ms == 0) {
        cyw43_arch_poll();
        if (!recv_ready) {
            return WIFI_TIMEOUT;
        }
    } else {
        // Blocking with timeout
        while (!recv_ready) {
            cyw43_arch_poll();
            sleep_ms(1);
            
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time;
            if (elapsed >= timeout_ms) {
                return WIFI_TIMEOUT;
            }
        }
    }
    
    if (recv_len > max_len) {
        printf("Received data too large (%zu > %zu)\n", recv_len, max_len);
        recv_ready = false;
        return WIFI_ERROR;
    }
    
    memcpy(buffer, recv_buffer, recv_len);
    size_t len = recv_len;
    recv_ready = false;
    
    return (int)len;
}