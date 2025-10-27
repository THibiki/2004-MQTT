#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "mqttsn_client.h"

#define WIFI_SSID "Iris"
#define WIFI_PSK  "S523515C"
#define GW_IP     "192.168.1.52"
#define GW_PORT   1884

static const char* link_to_str(int s) {
    switch (s) {
        case CYW43_LINK_DOWN:     return "DOWN";
        case CYW43_LINK_JOIN:     return "JOIN (auth/assoc in progress)";
        case CYW43_LINK_NOIP:     return "NO IP (DHCP in progress)";
        case CYW43_LINK_UP:       return "UP (connected + IP)";
        case CYW43_LINK_FAIL:     return "FAIL (general)";
        case CYW43_LINK_NONET:    return "NO NETWORK (SSID not found)";
        case CYW43_LINK_BADAUTH:  return "BAD AUTH (password?)";
        default:                  return "UNKNOWN";
    }
}

static void wifi_connect(void){
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_SINGAPORE)) {
        printf("WiFi init failed\n");
        while (true) { sleep_ms(1000); }
    }

    cyw43_arch_enable_sta_mode();

    printf("Connecting to WiFi SSID=%s ...\n", WIFI_SSID);
    int err = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID, WIFI_PSK, CYW43_AUTH_WPA2_AES_PSK, 60000
    );

    if (err) {
        int ls = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        printf("failed to connect (err=%d, link=%d %s)\n", err, ls, link_to_str(ls));
        while (true) { sleep_ms(1000); }
    }

    // Wait for DHCP to complete
    for (int i = 0; i < 150; ++i) {
        int ls = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
        printf("link=%d %s\n", ls, link_to_str(ls));
        if (ls == CYW43_LINK_UP) {
            const ip4_addr_t* ip = netif_ip4_addr(cyw43_state.netif);
            printf("IP: %s\n", ip4addr_ntoa(ip));
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            printf("WiFi OK, IP ready\n");
            return;
        }
        sleep_ms(200);
    }
    
    printf("DHCP timed out (still NOIP)\n");
    while (true) { sleep_ms(1000); }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Booting...\n");

    wifi_connect();

    mqttsn_client_t cli;
    if (!mqttsn_init(&cli, GW_IP, GW_PORT, "pico-1", 30)) {
        printf("MQTT-SN init failed\n");
        while (true) { sleep_ms(1000); }
    }
    if (!mqttsn_connect(&cli, true)) {
        printf("CONNECT failed\n");
        while (true) { sleep_ms(1000); }
    }
    printf("CONNACK OK\n");

    uint16_t tid = 0;
    if (!mqttsn_register(&cli, "sensors/pico-1/temp", &tid)) {
        printf("REGISTER failed\n");
        while (true) { sleep_ms(1000); }
    }
    printf("REGACK OK, topicId=%u\n", tid);

    // demo loop: publish QoS0 every 2s
    for(;;){
        char payload[16];
        float v = 24.0f + (to_ms_since_boot(get_absolute_time())/1000)%10;
        int n = snprintf(payload, sizeof(payload), "%.1f", v);
        if (!mqttsn_publish_qos0(&cli, tid, payload, (uint16_t)n)) {
            printf("PUBLISH fail\n");
        } else {
            printf("PUBLISHED: %s\n", payload);
        }
        sleep_ms(2000);
    }
}