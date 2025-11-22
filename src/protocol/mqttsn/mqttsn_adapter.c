// mqttsn_adapter.c - SIMPLE VERSION
#include "protocol/mqttsn/mqttsn_adapter.h"
#include "drivers/udp_driver.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdbool.h>

int mqttsn_transport_open(uint16_t local_port){
    return wifi_udp_create(local_port);
}

int mqttsn_transport_send(const char *dest_ip, uint16_t dest_port, const uint8_t *data, size_t len){
    return wifi_udp_send(dest_ip, dest_port, data, len);
}

int mqttsn_transport_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms){
    return wifi_udp_receive(buffer, max_len, timeout_ms);
}

void mqttsn_transport_close(void){
    wifi_udp_close();
}

uint32_t mqttsn_get_time_ms(void) {
    return to_ms_since_boot(get_absolute_time());
}