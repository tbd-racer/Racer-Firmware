cmake_minimum_required(VERSION 3.13)

# set this to allow the firmware init function to find the board dir
set(BOARD_HEADER_DIR ${CMAKE_SOURCE_DIR}/board)

# Add boards folder to PICO_BOARD_HEADER_DIRS 
list(APPEND PICO_BOARD_HEADER_DIRS ${BOARD_HEADER_DIR})

# The Target Board
# This also impacts the upload_tool for automatic device selection
set(PICO_BOARD example)

# Include global definitions
execute_process(COMMAND git rev-parse --show-cdup
	WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	OUTPUT_VARIABLE relative_path_to_repository_root OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
get_filename_component(REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/${relative_path_to_repository_root}" ABSOLUTE)
include(${REPO_DIR}/global_defs.cmake)

# Define Project
project(example C CXX ASM)
titan_firmware_init()

# Define executable
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
add_executable(example
	${SOURCES}
)
target_include_directories(example PUBLIC include)

# Set version: major minor (see titan/version.h for more info)
titan_generate_version_tag(example 1 0)

# Configure pico-sdk
pico_enable_stdio_uart(example 1)
pico_set_program_name(example "Power Board Firmware")

# Enable the 'make upload' and 'make serial' commands for project
# titan_use_upload_tool(example)
# titan_enable_bootloader(example can ${BOARD_HEADER_DIR})

# Define linking and targets
pico_add_extra_outputs(example)
target_link_libraries(example PUBLIC
	titan_dual_stdio_usb
	titan_logger	  # A nice little logging library
	driver_led
	pico_stdlib		  # Pico SDK Standard Library
)
