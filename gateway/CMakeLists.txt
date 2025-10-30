cmake_minimum_required(VERSION 3.13)
project(mqtt_sn_gateway C)

set(CMAKE_C_STANDARD 99)

# Find Paho MQTT C library using CMake config
find_package(eclipse-paho-mqtt-c REQUIRED)

add_executable(mqtt_sn_gateway
    main.c
    mqtt_paho.c
)

target_include_directories(mqtt_sn_gateway PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mqtt_sn_gateway PRIVATE 
    eclipse-paho-mqtt-c::paho-mqtt3c
)
