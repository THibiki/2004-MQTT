// udp_driver.c - UDP Socket Wrapper for MQTT-SN

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <lwip/udp.h>
#include <lwip/netif.h>
#include <lwip/ip_addr.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "udp_driver.h"
#include "network_errors.h"

// UDP State
static struct udp_pcb *udp_pcb = NULL;
static uint8_t *recv_buffer = NULL;
static size_t recv_len = 0;
static bool data_received = false;
// Persistent buffer for packets - always ready to receive
static uint8_t persistent_buffer[256];
static size_t persistent_buffer_len = 0;
static bool persistent_buffer_ready = false;

// Callback for UDP receives
static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                               const ip_addr_t *addr, u16_t port) {
    if (p != NULL) {
        if (recv_buffer != NULL) {
            // Copy data up to the available buffer size
            size_t copy_len = p->len < recv_len ? p->len : recv_len;
            memcpy(recv_buffer, p->payload, copy_len);
            // Update recv_len to the actual amount copied
            recv_len = copy_len;
            data_received = true;
        } else {
            // No active receive buffer, store in persistent buffer
            size_t copy_len = p->len < sizeof(persistent_buffer) ? p->len : sizeof(persistent_buffer);
            memcpy(persistent_buffer, p->payload, copy_len);
            persistent_buffer_len = copy_len;
            persistent_buffer_ready = true;
        }
        pbuf_free(p);
    }
}


int wifi_udp_create(uint16_t local_port){
    // Close existing PCB if open
    if (udp_pcb != NULL){
        printf("[INFO] Closing existing UDP sockets\n");
        udp_remove(udp_pcb);
        udp_pcb = NULL;
    }

    // Create new UDP PCB
    udp_pcb = udp_new();
    if(udp_pcb == NULL){
        printf("[ERROR] Failed to create UDP PCB.\n");
        return WIFI_ENOMEM; // memory allocation failed
    }

    // Bind to IP_ADDR_ANY to receive all packets on this port
    err_t err = udp_bind(udp_pcb, IP_ADDR_ANY, local_port);
    if (err != ERR_OK){
        printf("[ERROR] Failed to bind UDP PCB to port %d (Error: %d)\n", local_port, err);
        udp_remove(udp_pcb);
        udp_pcb = NULL;
        
        // Map lwip error to custom error code
        if (err == ERR_USE){
            printf("[INFO] UDP Port already in use\n");
            return WIFI_ESOCKET;
        } else if (err == ERR_MEM){
            return WIFI_ENOMEM;
        } else {
            return WIFI_ESOCKET;
        }
    }

    // Set receive callback
    udp_recv(udp_pcb, udp_recv_callback, NULL);

    printf("[INFO] UDP Socket created and bound to port %d\n", local_port);
    return WIFI_OK;                        
}

int wifi_udp_send(const char *dest_ip, uint16_t dest_port, 
    const uint8_t *data, size_t len){
        if (udp_pcb == NULL){
            printf("[ERROR] UDP send failed: socket not created.\n");
            return WIFI_ESOCKET;
        }

        if (dest_ip == NULL || data == NULL || len == 0){
            printf("[UDP] Send Failed: Invalid Parameters\n");
            return WIFI_EINVAL;
        }

        if (dest_port == 0){
            printf("[UDP] Send Failed: Invalid Port (0)\n");
            return WIFI_EINVAL;
        }

        ip_addr_t dest_addr;
        if (!ip4addr_aton(dest_ip, &dest_addr)){
            printf("[UDP] Send Failed: Invalid IP Address '%s'\n", dest_ip);
            return WIFI_EINVAL;
        }

        // Allocate packet buffer
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
        if (p == NULL){
            printf("[UDP] Send Failed: Could not allocate pbuf (%zu bytes)\n", len);
            return WIFI_ENOMEM;
        }

        memcpy(p->payload, data, len);

        err_t err = udp_sendto(udp_pcb, p, &dest_addr, dest_port);
        pbuf_free(p);

        if (err != ERR_OK){
            printf("[UDP] Send Failed: udp_sendto error %d\n", err);

            switch (err){
                case ERR_RTE:
                    printf("[UDP] No Route to %s\n", dest_ip);
                    return WIFI_ENOROUTE;
                case ERR_MEM:
                    return WIFI_ENOMEM;
                case ERR_BUF:
                    return WIFI_ENOMEM;
                default:
                    return WIFI_ESOCKET;
            }
        }

        printf("[UDP] Sent %zu bytes to %s:%d\n", len, dest_ip, dest_port);
        return WIFI_OK;
}

int wifi_udp_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
    // Treat all failures here as not connected
    if (udp_pcb == NULL){
        printf("[UDP] Received failed: socket not created\n");
        return WIFI_ESOCKET;
    }

    if (buffer == NULL || max_len == 0){
        printf("[UDP] Received failed: Invalid Buffer\n");
        return WIFI_EINVAL;
    }

    // Check if we have data in persistent buffer first
    if (persistent_buffer_ready) {
        size_t copy_len = persistent_buffer_len < max_len ? persistent_buffer_len : max_len;
        memcpy(buffer, persistent_buffer, copy_len);
        persistent_buffer_ready = false;
        persistent_buffer_len = 0;
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


void wifi_udp_close(void){
    if (udp_pcb != NULL){
        printf("[UDP] Closing socket...\n");
        udp_remove(udp_pcb);
        udp_pcb = NULL;
    }
}

bool is_udp_open(void){
    return (udp_pcb != NULL);
}