cmake_minimum_required(VERSION 3.13)

set(NAME steel_battalion_controller_fw)

set(PICO_BOARD none)

add_compile_definitions(PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H=1 PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)
set(TUSB_GAMEPAD_ROOT ${CMAKE_CURRENT_LIST_DIR}/lib/tusb_gamepad)

add_subdirectory(${TUSB_GAMEPAD_ROOT})
target_include_directories(tusb_gamepad PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(tusb_gamepad PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/src)
target_include_directories(tusb_gamepad PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/hw)

#initialize the Raspberry Pi Pico SDK
pico_sdk_init()

file(GLOB_RECURSE SOURCES main.c led.c sn74hc165.c tle5011.c tle5012.c crc.c as5600.c spi_half_duplex.h)

#rest of your project
add_executable(${NAME} ${SOURCES})

# generate pio headers
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(${NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

target_include_directories(${NAME} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/src
	${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/hw
	${CMAKE_CURRENT_LIST_DIR})

# add pico_stdlib library which aggregates commonly used features
target_link_libraries(${NAME}
	pico_stdlib
	tinyusb_device
	tinyusb_board
	tusb_gamepad
    hardware_adc
	hardware_pio
	hardware_spi
	hardware_dma
	hardware_i2c
)

# create map/bin/hex/uf2 file in addition to ELF
pico_add_extra_outputs(${NAME})
