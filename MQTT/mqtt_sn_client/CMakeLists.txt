# MQTT-SN Client CMakeLists.txt

# Build the main MQTT-SN client example
add_executable(mqtt_sn_client
        main.c
        mqtt_sn_client.c
        mqtt_sn_messages.c
        )

# Add include directories for lwIP configuration files
target_include_directories(mqtt_sn_client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# pull in common dependencies
target_link_libraries(mqtt_sn_client 
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_nosys
    )

# Define NO_SYS for lwIP
target_compile_definitions(mqtt_sn_client PRIVATE 
    NO_SYS=1
    PICO_CYW43_ARCH_THREADSAFE_BACKGROUND=1
)

# enable usb output, disable uart output
pico_enable_stdio_usb(mqtt_sn_client 1)
pico_enable_stdio_uart(mqtt_sn_client 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(mqtt_sn_client)
