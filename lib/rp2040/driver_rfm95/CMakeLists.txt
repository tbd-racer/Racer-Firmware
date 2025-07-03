add_library(rfm95 INTERFACE)

target_sources(rfm95 INTERFACE 
    ${CMAKE_CURRENT_LIST_DIR}/src/rfm9x.c
)

target_link_libraries(rfm95 INTERFACE
    pico_stdlib
    hardware_spi
    hardware_gpio
)   

target_include_directories(rfm95 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)


