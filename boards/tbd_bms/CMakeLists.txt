cmake_minimum_required(VERSION 3.13)

# The Target Board
# These can be found in lib/uwrt_boards/include/boards or be set to pico
# Is is preferred to define pins in the board header file and refer to that name rather than hardcoding names
# This also impacts the upload_tool for automatic device selection
set(PICO_BOARD tbd_bms)

# Load custom board definitions (must occur before SDK init)
# Add boards folder to PICO_BOARD_HEADER_DIRS 
list(APPEND PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/board)

# Include global definitions
execute_process(COMMAND git rev-parse --show-cdup
	WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
	OUTPUT_VARIABLE relative_path_to_repository_root OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE)
get_filename_component(REPO_DIR "${CMAKE_CURRENT_LIST_DIR}/${relative_path_to_repository_root}" ABSOLUTE)
include(${REPO_DIR}/global_defs.cmake)

# Define Project
project(smart_battery C CXX ASM)
titan_firmware_init()

# Define executable
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_SOURCE_DIR} "src/*.c")
add_executable(smart_battery
	${SOURCES}
)
target_include_directories(smart_battery PUBLIC include)

# Configure pico-sdk
pico_enable_stdio_uart(smart_battery 1)
pico_set_program_name(smart_battery "Smart Battery Firmware")

# Enable the 'make upload' and 'make serial' commands for project
# titan_use_upload_tool(smart_battery)
# titan_enable_bootloader(smart_battery can)

# Define linking and targets
pico_add_extra_outputs(smart_battery)
target_link_libraries(smart_battery PUBLIC
	driver_async_i2c  # Async I2C Driver
	driver_led		  # Status LED support
	driver_ssd1306	  # SSD1306 LCD Support
	titan_logger	  # A nice little logging library
	titan_safety	  # Handles critical code (Watchdog, Kill Switch, Fault Reporting)
	micro_ros_can	  # Micro ros support (supports _can, _usb, _eth)
	hardware_i2c
	pico_stdlib		  # Pico SDK Standard Library
)