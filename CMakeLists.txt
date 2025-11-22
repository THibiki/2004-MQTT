cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico_w)

# Import the Pico SDK
include(pico_sdk_import.cmake)

project(picow_network C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Optional: detect Paho MQTT-SN library in repository (keep its sources untouched)
set(PAHO_DIR_ROOT "${CMAKE_CURRENT_LIST_DIR}/lib/paho.mqtt-sn.embedded-c")
set(PAHO_DIR_BUILD "${CMAKE_CURRENT_LIST_DIR}/build/lib/paho.mqtt-sn.embedded-c")

if (EXISTS "${PAHO_DIR_ROOT}")
    set(PAHO_DIR "${PAHO_DIR_ROOT}")
elseif (EXISTS "${PAHO_DIR_BUILD}")
    set(PAHO_DIR "${PAHO_DIR_BUILD}")
else()
    set(PAHO_DIR "")
endif()

if (PAHO_DIR)
    message(STATUS "Paho MQTT-SN detected at ${PAHO_DIR}")
    file(GLOB PAHO_MQTTSN_PACKET_SRCS "${PAHO_DIR}/MQTTSNPacket/src/*.c")
    file(GLOB PAHO_MQTTSN_CLIENT_SRCS "${PAHO_DIR}/MQTTSNClient/src/*.c")

    add_library(mqttsn_paho STATIC
        ${PAHO_MQTTSN_PACKET_SRCS}
        ${PAHO_MQTTSN_CLIENT_SRCS}
    )

    target_include_directories(mqttsn_paho PUBLIC
        ${PAHO_DIR}/MQTTSNPacket/src
        ${PAHO_DIR}/MQTTSNClient/src
        ${PAHO_DIR}
    )
else()
    message(STATUS "Paho MQTT-SN not found; build will proceed without MQTT-SN support")
endif()


# Add FatFs library
add_library(fatfs STATIC
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source/ff.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source/diskio_sdcard.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source/ffsystem.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source/ffunicode.c
)

target_include_directories(fatfs PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/lib/fatfs/source
)

# Project libraries
set(PROJECT_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/include)

# Drivers: WiFi, UDP, SD card, block transfer
add_library(drivers STATIC
    src/drivers/wifi_driver.c
    src/drivers/udp_driver.c
    src/drivers/sd_card.c
    src/drivers/block_transfer.c
)

target_include_directories(drivers PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(drivers PUBLIC
    pico_stdlib
    pico_lwip
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_spi
    hardware_gpio
    hardware_adc
    fatfs
)

# MQTT-SN core (your own client + adapter)
add_library(mqttsn_core STATIC
    src/protocol/mqttsn/mqttsn_client.c
    src/protocol/mqttsn/mqttsn_adapter.c
)

target_include_directories(mqttsn_core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(mqttsn_core PUBLIC
    drivers
)

# If Paho was detected, link the library and provide a compile macro so
# example code can conditionally include Paho headers.
if (PAHO_DIR)
    target_link_libraries(mqttsn_core PUBLIC mqttsn_paho)
    target_compile_definitions(mqttsn_core PUBLIC HAVE_PAHO=1)
endif()


# Main application
add_executable(picow_network
    src/app/main.c
)


# Application needs access to src and include directories
target_include_directories(picow_network PUBLIC
    ${PROJECT_INCLUDE_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Enable stdio
pico_enable_stdio_usb(picow_network 1)
pico_enable_stdio_uart(picow_network 0)

# Generate UF2, bin, etc.
pico_add_extra_outputs(picow_network)

# Link libraries + Pico SDK components
target_link_libraries(picow_network
    drivers
    mqttsn_core
    fatfs

    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background

    hardware_spi
    hardware_gpio
    hardware_adc
)