add_library(driver_ws2812 INTERFACE)

target_sources(driver_ws2812 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.c
)
# TODO: Make not generate pioasm unless target included
pico_generate_pio_header(driver_ws2812 ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

target_include_directories(driver_ws2812 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(driver_ws2812 INTERFACE
    hardware_pio
    hardware_dma
)
