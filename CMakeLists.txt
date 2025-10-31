cmake_minimum_required(VERSION 3.13...4.0)

# Set board BEFORE including SDK
set(PICO_BOARD pico_w)

# Include SDK BEFORE project()
include(pico_sdk_import.cmake)

# Declare project
project(wifi_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialize SDK
pico_sdk_init()

# Automatically find all MQTT-SN source files
file(GLOB MQTT_SN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/lib/mqtt-sn/*.c")

# Print what files were found (for debugging)
message(STATUS "MQTT-SN source files found:")
foreach(file ${MQTT_SN_SOURCES})
    message(STATUS "  ${file}")
endforeach()

# Add MQTT-SN library
add_library(mqtt_sn INTERFACE)
target_sources(mqtt_sn INTERFACE ${MQTT_SN_SOURCES})
target_include_directories(mqtt_sn INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/lib/mqtt-sn
)

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

# Main executable
add_executable(wifi_project
    main.c
    wifi_driver.c
    mqtt_sn_client.c
    block_transfer.c
    sd_card.c
)

pico_set_program_name(wifi_project "wifi_project")
pico_set_program_version(wifi_project "0.1")

target_include_directories(wifi_project PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_enable_stdio_usb(wifi_project 1)
pico_enable_stdio_uart(wifi_project 0)

# Link libraries including MQTT-SN and FatFs
target_link_libraries(wifi_project
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    hardware_adc
    hardware_spi
    hardware_gpio
    mqtt_sn
    fatfs
)

pico_add_extra_outputs(wifi_project)
