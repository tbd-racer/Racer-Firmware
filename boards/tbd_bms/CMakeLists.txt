cmake_minimum_required(VERSION 3.13)

# set this to allow the firmware init function to find the board dir
set(BOARD_HEADER_DIR ${CMAKE_SOURCE_DIR}/board)

# Add boards folder to PICO_BOARD_HEADER_DIRS 
list(APPEND PICO_BOARD_HEADER_DIRS ${BOARD_HEADER_DIR})

# The Target Board
# This also impacts the upload_tool for automatic device selection
set(PICO_BOARD tbd_bms)

# Include global definitions
execute_process(COMMAND git rev-parse --show-cdup
	WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	OUTPUT_VARIABLE relative_path_to_repository_root OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
get_filename_component(REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/${relative_path_to_repository_root}" ABSOLUTE)
include(${REPO_DIR}/global_defs.cmake)

# Define Project
project(tbd_bms C CXX ASM)
titan_firmware_init()

# Define executable
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
add_executable(tbd_bms
	${SOURCES}
)
target_include_directories(tbd_bms PUBLIC include)

# Set version: major minor (see titan/version.h for more info)
titan_generate_version_tag(tbd_bms 1 0)

# Configure pico-sdk
pico_enable_stdio_uart(tbd_bms 1)
pico_set_program_name(tbd_bms "BMS Firmware")

# Enable the 'make upload' and 'make serial' commands for project
# titan_use_upload_tool(tbd_bms)
# titan_enable_bootloader(tbd_bms can ${BOARD_HEADER_DIR})

# Define linking and targets
pico_add_extra_outputs(tbd_bms)
target_link_libraries(tbd_bms PUBLIC
	driver_async_i2c  # Async I2C Driver for the display
	driver_led		  # Status LED Support
	driver_ssd1306	  # SSD1306 LCD Support
	driver_bq40z80    # BQ40z80 BMS Support
	titan_logger	  # A nice little logging library
	titan_safety	  # Handles critical code (Watchdog, Kill Switch, Fault Reporting)
	micro_ros_can	  # Micro ros support (supports _can, _usb, _eth)
	hardware_i2c
	pico_stdlib		  # Pico SDK Standard Library
)
