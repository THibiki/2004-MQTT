cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

include(pico_sdk_import.cmake)

project(pico_mqttsn C CXX ASM)

pico_sdk_init()

add_executable(pico_mqttsn
  src/main.c
  src/net_udp.c
  src/mqttsn_client.c
)

target_include_directories(pico_mqttsn PRIVATE src)

pico_enable_stdio_usb(pico_mqttsn 1)
pico_enable_stdio_uart(pico_mqttsn 0)

target_link_libraries(pico_mqttsn 
    pico_stdlib 
    pico_cyw43_arch_lwip_threadsafe_background
)

pico_add_extra_outputs(pico_mqttsn)
