add_library(driver_ads7828 INTERFACE)

target_sources(driver_ads7828 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src/ads7828.c
)

target_include_directories(driver_ads7828 INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(driver_ads7828 INTERFACE
    driver_async_i2c
)