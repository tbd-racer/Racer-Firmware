add_library(driver_rfm95 INTERFACE)

target_sources(driver_rfm95 INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}/src/rfm9x.c
)

target_link_libraries(driver_rfm95 INTERFACE
    pico_stdlib
    hardware_spi
    hardware_gpio
)   

target_include_directories(driver_rfm95 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)


