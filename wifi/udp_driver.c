// udp_driver.c - UDP Socket Wrapper for MQTT-SN

#include <stdio.h>
#include <string.h>
#include <lwip/udp.h>
#include <lwip/netif.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "udp_driver.h"
#include "network_errors.h"

// UDP State
static struct udp_pcb *udp_pcb = NULL;
static uint8_t *recv_buffer = NULL;
static size_t recv_len = 0;
static bool data_received = false;

// Callback for UDP receives
static void udp_recv_callback(void *args, struct udp_pcb *pcb, struct pbuf *p, 
    const ip_addr_t *addr, u16_t port){

        if (p!= NULL){
            printf("[INFO] UDP callback: recevied %d bytes from %s:%n\n", 
                p->len, ip4addr_ntoa(addr), port);
            
            if (recv_buffer != NULL){
                // Copy data up to available buffer size
                size_t copy_len = p->len < recv_len ? p->len : recv_len;
                memcpy(recv_buffer, p->payload, copy_len);
                recv_len = copy_len;     // Updates actual bytes copied
                printf("[INFO] Copied %zu bytes to buffer\n", copy_len);
            } else {
                printf("[INFO] No receive buffer set,, dropping packet\n");
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

    // Blind to local port
    err_t err = udp_bind(udp_pcb, IP_ADDR_ANY, local_port);
    if (err != ERR_OK){
        printf("[ERROR] Failed to blind UDP PCB to port %d (Error: %d)\n", local_port, err);
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

    udp_recv(udp_pcb, udp_recv_callback, NULL);

    printf("[INFO] UDP Socket created and bound to port $d\n", local_port);
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

        // Alloate packet buffer
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

int wifi_udp_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms){

    if (udp_pcb == NULL){
        printf("[UDP] Received failed: socket not created\n");
        return WIFI_ESOCKET;
    }

    if (buffer == NULL || max_len == 0){
        printf("[UDP] Received failed: Invalid Buffer\n");
    }

    recv_buffer = buffer;
    recv_len = max_len;
    data_received = false;

    if (timeout_ms == 0){
        cyw43_arch_poll();

        if (!data_received){
            recv_buffer = NULL;
            return 0;
        }

        return 0;
    } else {
        absolute_time_t timeout_time = make_timeout_time_ms(timeout_ms);

        while (!data_received){
            cyw43_arch_poll();

            if (time_reached(timeout_time)) {
                recv_buffer = NULL;
                printf("[UDP] Recieve timeout after %lu ms\n", timeout_ms);
                return WIFI_ETIMEDOUT;
            }

            sleep_ms(1);    // small sleep to avoid busy-waiting
        }

        // Data Recieved
        int bytes_received = recv_len;
        recv_buffer = NULL;
        data_received = false;

        printf("[UDP] Received %d bytes (blocking, %lu ms timeout)\n", bytes_received, timeout_ms);
        return bytes_received;
    }
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