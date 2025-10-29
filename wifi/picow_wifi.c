#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "wifi_config.h"

int wifi_connect(void){

    printf("\n====Connecting to Wifi====\n");

    if(cyw43_arch_init_with_country(CYW43_COUNTRY_SINGAPORE)){
        printf("Failed to Initalise \n");
        return 1;
    }

    printf("Initalised\n");

    cyw43_arch_enable_sta_mode();

    printf("Attempting Connection...\n");
    int retry_count = 0;
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){
        printf(".");
        retry_count++;
        if (retry_count >= 5){
            printf("Failed to connect to SSID=%s after %d attempts\n", WIFI_SSID, retry_count);
            return -1;
        }
    }

    printf("SSID, %s Connected!\n", WIFI_SSID);

    // Print IP address
    struct netif *netif = netif_default;
    if (netif != NULL){
        printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
        printf("Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
        printf("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));
    }

    return 0;
}

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

    // WiFi Connection Initalisation
    if (wifi_connect() != 0){
        printf("WiFi Connection Failed...\n");
        while (1){
            sleep_ms(2000);
        }
    }

    sleep_ms(2000);

    // MQTT-SN Gatewat Initalisation
    printf("==== Connecting to MQTT-SN Gateway ====\n");
    printf("Gateway: %s:%d\n", GATEWAY_IP, GATEWAY_PORT);

    if (mqttsn_connect() != 0){
        printf("Failed to send CONNECT packet\n");
    }

    // Wait for CONNACK
    sleep_ms(2000);
}