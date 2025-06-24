cmake_minimum_required(VERSION 3.13)

# Include global definitions
execute_process(COMMAND git rev-parse --show-cdup
	WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	OUTPUT_VARIABLE relative_path_to_repository_root OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
get_filename_component(REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/${relative_path_to_repository_root}" ABSOLUTE)
include(${REPO_DIR}/global_defs.cmake)

project(hello C CXX ASM)

# Setup sdk
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
# titan_firmware_init(${BOARD_HEADER_DIR})

add_executable(hello hello.c)

# pull in common dependencies
target_link_libraries(hello pico_stdlib
					        radio_head)

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(hello pico_cyw43_arch_none)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(hello)

# add url via pico_set_program_url
# example_auto_set_url(hello)