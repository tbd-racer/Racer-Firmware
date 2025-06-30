project(rfm95 C)

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
    $<INSTALL_INTERFACE:include/>
)


