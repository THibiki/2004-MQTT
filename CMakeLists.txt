cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(picow_network C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico_w)

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

  add_library(mqttsn_paho STATIC ${PAHO_MQTTSN_PACKET_SRCS} ${PAHO_MQTTSN_CLIENT_SRCS})
  target_include_directories(mqttsn_paho PUBLIC ${PAHO_DIR}/MQTTSNPacket/src ${PAHO_DIR}/MQTTSNClient/src ${PAHO_DIR})
else()
  message(STATUS "Paho MQTT-SN not found; build will proceed without MQTT-SN support")
endif()


add_executable(picow_network
  main.c
  wifi_driver.c
  udp_driver.c
  mqttsn_adapter.c
  mqttsn_client.c
)

pico_enable_stdio_usb(picow_network 1)
pico_enable_stdio_uart(picow_network 0)

pico_add_extra_outputs(picow_network)

target_include_directories(picow_network PRIVATE ${CMAKE_CURRENT_LIST_DIR} )

target_link_libraries(picow_network PRIVATE
    pico_cyw43_arch_lwip_threadsafe_background 
    pico_stdlib
)

# If Paho was detected, link the library and provide a compile macro so
# example code can conditionally include Paho headers.
if (EXISTS "${PAHO_DIR}")
  target_link_libraries(picow_network PRIVATE mqttsn_paho)
  target_compile_definitions(picow_network PRIVATE HAVE_PAHO=1)
endif()