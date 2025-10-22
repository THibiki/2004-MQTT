#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "mqttsn_client.h"

#define WIFI_SSID "SM-Hotspot"
#define WIFI_PSK  "shimin0323"
#define GW_IP     "192.168.56.1"   // your laptop IP
#define GW_PORT   1884

int main() {
  stdio_init_all();
  sleep_ms(2000);
  printf("Booting...\n");

  if (cyw43_arch_init_with_country(CYW43_COUNTRY_SINGAPORE)) {
    printf("WiFi init failed\n");
    return -1;
  }
  cyw43_arch_enable_sta_mode();

  printf("Connecting WiFi…\n");
  int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PSK,
          CYW43_AUTH_WPA2_AES_PSK, 30000);

  int ls = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
  printf("connect rc=%d, link_status=%d\n", rc, ls);

  if (rc) {
      printf("WiFi connect failed\n");
      return -1;
  }
  printf("WiFi OK, IP ready\n");

  mqttsn_client_t cli;
  if (!mqttsn_init(&cli, GW_IP, GW_PORT, "pico-1", 30)) {
    printf("MQTT-SN init failed\n");
    return -1;
  }
  if (!mqttsn_connect(&cli, true)) {
    printf("CONNECT failed\n");
    return -1;
  }
  printf("CONNACK OK\n");

  uint16_t tid = 0;
  if (!mqttsn_register(&cli, "sensors/pico-1/temp", &tid)) {
    printf("REGISTER failed\n"); return -1;
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
    cyw43_arch_poll();
    sleep_ms(2000);
  }
}
