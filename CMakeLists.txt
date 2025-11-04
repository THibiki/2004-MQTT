cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picow_network C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Add MQTT-SN source files
# set(MQTTSN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../paho.mqtt-sn.embedded-c/MQTTSNPacket/src")


add_executable(picow_network
  main.c
  wifi_driver.c
  udp_driver.c
)

pico_enable_stdio_usb(picow_network 1)
pico_enable_stdio_uart(picow_network 0)

pico_add_extra_outputs(picow_network)

target_include_directories(picow_network PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

target_link_libraries(picow_network 
    pico_cyw43_arch_lwip_threadsafe_background 
    pico_stdlib
)