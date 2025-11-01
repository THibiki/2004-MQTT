cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picow_wifi C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Add MQTT-SN source files
set(MQTTSN_DIR "${CMAKE_CURRENT_LIST_DIR}/../../paho.mqtt-sn.embedded-c/MQTTSNPacket/src")

add_executable(picow_wifi
  picow_wifi.c
  wifi.c
)

pico_enable_stdio_usb(picow_wifi 1)
pico_enable_stdio_uart(picow_wifi 1)

pico_add_extra_outputs(picow_wifi)

target_include_directories(picow_wifi PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

target_link_libraries(picow_wifi 
    pico_cyw43_arch_lwip_threadsafe_background 
    pico_stdlib
)