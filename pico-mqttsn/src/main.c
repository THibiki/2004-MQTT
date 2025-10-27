#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"

/* Added includes for UDP send */
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"

#define WIFI_SSID "Hibiki"
#define WIFI_PSK  ""   // empty -> OPEN hotspot. Put password here if using WPA2.

/* STATIC IP fallback settings */
#define STATIC_IP_A 10
#define STATIC_IP_B 117
#define STATIC_IP_C 49
#define STATIC_IP_D 200
#define STATIC_GW_A 10
#define STATIC_GW_B 117
#define STATIC_GW_C 49
#define STATIC_GW_D 177
#define STATIC_NM_A 255
#define STATIC_NM_B 255
#define STATIC_NM_C 255
#define STATIC_NM_D 0

/* Destination for UDP test - CHANGE to your laptop's IP and port */
static const char *DEST_IP = "10.117.49.177";
static const uint16_t DEST_PORT = 5005;

/* Helper to send a UDP test packet */
static void send_udp_test(const char *msg) {
    printf("DEBUG: netif ip=%s, gw=%s, dest=%s\n",
       ip4addr_ntoa(netif_ip4_addr(cyw43_state.netif)),
       ip4addr_ntoa(netif_gw4_addr(cyw43_state.netif)),
       DEST_IP);
    struct udp_pcb *pcb = udp_new();
    if (!pcb) {
        printf("udp_new failed\n");
        return;
    }
    ip_addr_t dest;
    if (!ipaddr_aton(DEST_IP, &dest)) {
        printf("ipaddr_aton failed for %s\n", DEST_IP);
        udp_remove(pcb);
        return;
    }

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, strlen(msg), PBUF_RAM);
    if (!p) {
        printf("pbuf_alloc failed\n");
        udp_remove(pcb);
        return;
    }
    memcpy(p->payload, msg, strlen(msg));
    err_t e = udp_sendto(pcb, p, &dest, DEST_PORT);
    printf("udp_sendto -> err=%d\n", e);
    pbuf_free(p);
    udp_remove(pcb);
}

static const char* link_to_str(int s) {
    switch (s) {
    case CYW43_LINK_DOWN:    return "DOWN";
    case CYW43_LINK_JOIN:    return "JOIN";
    case CYW43_LINK_NOIP:    return "NOIP";
    case CYW43_LINK_UP:      return "UP";
    case CYW43_LINK_FAIL:    return "FAIL";
    case CYW43_LINK_NONET:   return "NONET";
    case CYW43_LINK_BADAUTH: return "BADAUTH";
    default: return "UNK";
    }
}

int main(void) {
    stdio_init_all();
    sleep_ms(500);
    printf("\n--- WiFi static-fallback + UDP test boot ---\n");

    if (cyw43_arch_init_with_country(CYW43_COUNTRY_SINGAPORE)) {
        printf("cyw43 init failed\n");
        while (true) sleep_ms(1000);
    }
    cyw43_arch_enable_sta_mode();

    int auth = (strlen(WIFI_PSK) == 0) ? CYW43_AUTH_OPEN : CYW43_AUTH_WPA2_MIXED_PSK;
    printf("Connecting to SSID='%s' auth=%d ...\n", WIFI_SSID, auth);

    int err = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PSK, auth, 30000);
    printf("connect returned %d\n", err);

    if (cyw43_state.netif) {
        printf("Starting DHCP explicitly\n");
        dhcp_start(cyw43_state.netif);
    } else {
        printf("No cyw43 netif available after connect\n");
    }

    /* Wait up to 30s for DHCP to assign an IP */
    bool got_ip = false;
    for (int t = 0; t < 150; ++t) { // 150 * 200ms = 30s
        int ls = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        printf("status: %s\n", link_to_str(ls));
        if (ls == CYW43_LINK_BADAUTH) {
            printf("BAD AUTH - check SSID/password\n");
            break;
        }
        if (ls == CYW43_LINK_UP && cyw43_state.netif) {
            const ip4_addr_t* ip = netif_ip4_addr(cyw43_state.netif);
            if (ip && ip4_addr_isany(ip) == 0) {
                printf("GOT IP (DHCP): %s\n", ip4addr_ntoa(ip));
                got_ip = true;
                break;
            }
        }
        sleep_ms(200);
    }

    if (got_ip) {
        /* If DHCP succeeded, send UDP test */
        printf("Sending UDP test after DHCP...\n");
        printf("DEBUG: netif ip=%s, gw=%s, dest=%s\n",
            ip4addr_ntoa(netif_ip4_addr(cyw43_state.netif)),
            ip4addr_ntoa(netif_gw4_addr(cyw43_state.netif)),
            DEST_IP);
        send_udp_test("Hello from PicoW (DHCP)");
    } else if (cyw43_state.netif) {
        /* DHCP failed -> set static IP as fallback and then send UDP test */
        ip4_addr_t ip, gw, nm;
        IP4_ADDR(&ip, STATIC_IP_A, STATIC_IP_B, STATIC_IP_C, STATIC_IP_D);
        IP4_ADDR(&gw, STATIC_GW_A, STATIC_GW_B, STATIC_GW_C, STATIC_GW_D);
        IP4_ADDR(&nm, STATIC_NM_A, STATIC_NM_B, STATIC_NM_C, STATIC_NM_D);

        netif_set_addr(cyw43_state.netif, &ip, &nm, &gw);
        netif_set_up(cyw43_state.netif);
        printf("Set static IP: %s  GW:%s\n", ip4addr_ntoa(&ip), ip4addr_ntoa(&gw));
        printf("DEBUG: netif ip=%s, gw=%s, dest=%s\n",
            ip4addr_ntoa(netif_ip4_addr(cyw43_state.netif)),
            ip4addr_ntoa(netif_gw4_addr(cyw43_state.netif)),
            DEST_IP);
        printf("Sending UDP test after static IP...\n");
        send_udp_test("Hello from PicoW (STATIC)");
    } else {
        printf("No netif available; can't send UDP\n");
    }

    /* main loop: print netif IP every 5s */
    while (true) {
        if (cyw43_state.netif) {
            const ip4_addr_t* ip = netif_ip4_addr(cyw43_state.netif);
            if (ip) {
                printf("NETIF IP: %s\n", ip4addr_ntoa(ip));
            } else {
                printf("NETIF exists but ip==NULL\n");
            }
        } else {
            printf("NO netif\n");
        }
        sleep_ms(5000);
    }
}