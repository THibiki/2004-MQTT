cmake_minimum_required(VERSION 3.13)
project(mqtt_sn_gateway C)

set(CMAKE_C_STANDARD 99)

# Find Paho MQTT C (installed via vcpkg or system package)
find_package(PahoMqttC REQUIRED)

add_executable(mqtt_sn_gateway
    main.c
    mqtt_paho.c
)

target_include_directories(mqtt_sn_gateway PRIVATE ${PAHO_MQTT_C_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mqtt_sn_gateway PRIVATE ${PAHO_MQTT_C_LIBRARIES})
