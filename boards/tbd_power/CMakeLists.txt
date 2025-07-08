cmake_minimum_required(VERSION 3.13)

# set this to allow the firmware init function to find the board dir
set(BOARD_HEADER_DIR ${CMAKE_SOURCE_DIR}/board)

# Add boards folder to PICO_BOARD_HEADER_DIRS 
list(APPEND PICO_BOARD_HEADER_DIRS ${BOARD_HEADER_DIR})

# The Target Board
# This also impacts the upload_tool for automatic device selection
set(PICO_BOARD tbd_power)

# Include global definitions
execute_process(COMMAND git rev-parse --show-cdup
	WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	OUTPUT_VARIABLE relative_path_to_repository_root OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
get_filename_component(REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/${relative_path_to_repository_root}" ABSOLUTE)
include(${REPO_DIR}/global_defs.cmake)

# Define Project
project(tbd_power C CXX ASM)
titan_firmware_init()

# Define executable
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
add_executable(tbd_power
	${SOURCES}
)
target_include_directories(tbd_power PUBLIC include)

# Set version: major minor (see titan/version.h for more info)
titan_generate_version_tag(tbd_power 1 0)

# Configure pico-sdk
pico_enable_stdio_uart(tbd_power 1)
pico_set_program_name(tbd_power "Power Board Firmware")

# Enable the 'make upload' and 'make serial' commands for project
# titan_use_upload_tool(tbd_power)
# titan_enable_bootloader(tbd_power can ${BOARD_HEADER_DIR})

# Define linking and targets
pico_add_extra_outputs(tbd_power)
target_link_libraries(tbd_power PUBLIC
	driver_ads7828    # ADC library for power board
	driver_async_i2c  # Async I2C Driver for the display
	driver_led		  # Status LED Support
	driver_rfm95      # RFM 95 radio support

	titan_logger	  # A nice little logging library
	titan_safety	  # Handles critical code (Watchdog, Kill Switch, Fault Reporting)
	micro_ros_can	  # Micro ros support (supports _can, _usb, _eth)
	
	pico_stdlib		  # Pico SDK Standard Library
	pico_stdio_usb
)
