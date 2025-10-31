#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/ip_addr.h"

#include "wifi_config.h"

int wifi_connect(void){

    printf("Attempting Connection...\n");
    int retry_count = 0;
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000)){
        printf(".");
        retry_count++;
        if (retry_count >= 5){
            printf("Failed to connect to SSID=%s after %d attempts\n", WIFI_SSID, retry_count);
            printf("Please check if SSID=%s is available for connection\n", WIFI_SSID);
            return -1;
        }
    }

    printf("SSID=%s Connected!\n", WIFI_SSID);

    // Print pico w IP address
    struct netif *netif = netif_default;
    if (netif != NULL){
        printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
        printf("Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
        printf("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));
    }

    return 0;
}

static bool wifi_is_up(void){
    int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);

    switch (status){
        case CYW43_LINK_UP:      // Connected with IP address available
        case CYW43_LINK_NOIP:    // Connected but DHCP maybe not done yet
            return true;
        default:
            return false;       // No connection established
    }
}

int main(){
    stdio_init_all();
    sleep_ms(3000); // Provide time for serial monitor to connect

    printf("\n====Connecting to Wifi====\n");

    if(cyw43_arch_init_with_country(CYW43_COUNTRY_SINGAPORE)){
        printf("Failed to Initalise \n");
        return 1;
    }

    printf("Initalised\n");

    cyw43_arch_enable_sta_mode();

    // WiFi Connection Initalisation
    if (wifi_connect() != 0){
        printf("WiFi Connection Failed...\n");
        while (1){
            sleep_ms(2000);
        }
    }

    // Reconnect loop status
    absolute_time_t next_retry_allowed = get_absolute_time(); // now
    const uint32_t RETRY_COOLDOWN_MS = 5000; // 5s between attempts

    while (true){
        // Check WiFi link
        bool up = wifi_is_up();

        // WiFi is not connected
        if (!up){
            // Retry cooldown is up, try connecting again
            if (absolute_time_diff_us(get_absolute_time(), next_retry_allowed) <= 0){
                printf("[WiFi] LOST. Attempting Reconnection...");

                // WiFi Connection Initalisation
                if (wifi_connect() != 0){
                    printf("WiFi Connection Failed...\n");
                    while (1){
                        sleep_ms(2000);
                    }
                }

                // set next retry time in the future
                next_retry_allowed = make_timeout_time_ms(RETRY_COOLDOWN_MS);

            }
        }

        sleep_ms(50);  // pause to avoid busy loop (using CPU 100% of the time)

    }

    sleep_ms(2000);


}