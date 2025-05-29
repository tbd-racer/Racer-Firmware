add_library(driver_bq40z80 INTERFACE)

target_sources(driver_bq40z80 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/bq40z80.c
)

target_include_directories(driver_bq40z80 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(driver_bq40z80 INTERFACE
    hardware_clocks
    hardware_gpio
    hardware_irq
    hardware_i2c
)
