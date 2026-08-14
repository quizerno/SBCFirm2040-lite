cmake_minimum_required(VERSION 3.13)

set(NAME steel_battalion_controller_fw)
set(PICO_BOARD none)

add_compile_definitions(    PICO_BOOT_STAGE2_CHOOSE_GENERIC_03H=1 
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
    
    # --- ADD THESE THREE LINES TO FIX THE ERRORS ---
    PICO_DEFAULT_PIO_USB_DP_PIN=0
    BOARD_TUH_RHPORT=1
    BOARD_DEVICE_RHPORT=0)
	
	
# Initialize the SDK based on PICO_SDK_PATH
include ($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(${NAME} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)
set(TUSB_GAMEPAD_ROOT ${CMAKE_CURRENT_LIST_DIR}/lib/tusb_gamepad)

# Path to your pico_pio_usb library
set(PICO_PIO_USB_ROOT ${CMAKE_CURRENT_LIST_DIR}/lib/pico-pio-usb)

add_subdirectory(${TUSB_GAMEPAD_ROOT})
# Add pico_pio_usb subdirectory to build its state-machines
add_subdirectory(${PICO_PIO_USB_ROOT})

target_include_directories(tusb_gamepad PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(tusb_gamepad PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/src)
target_include_directories(tusb_gamepad PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/hw)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

set(SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/main.c 
    ${CMAKE_CURRENT_LIST_DIR}/led.c 
    ${CMAKE_CURRENT_LIST_DIR}/sn74hc165.c 
    ${CMAKE_CURRENT_LIST_DIR}/crc.c 
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

# Rest of your project
add_executable(${NAME} ${SOURCES})

# Generate pio headers
file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/generated)
pico_generate_pio_header(${NAME} ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/generated)

# --- FIXED: Used INTERFACE instead of PUBLIC since tinyusb_board is an INTERFACE target ---
if (TARGET tinyusb_board)
    target_include_directories(tinyusb_board INTERFACE ${PICO_PIO_USB_ROOT}/src)
endif()

target_include_directories(${NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/src
    ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb/hw
    ${CMAKE_CURRENT_LIST_DIR}
    ${PICO_PIO_USB_ROOT}/src
    ${PICO_PIO_USB_ROOT}
)

# Link libraries together
target_link_libraries(${NAME}
    pico_stdlib
    tinyusb_device
    tinyusb_host     # Enables TinyUSB Host APIs
    tinyusb_board
    tusb_gamepad
    pico_pio_usb     # Links the PIO bit-banging code
    hardware_adc
    hardware_pio
    hardware_spi
    hardware_dma
    hardware_i2c
    hardware_irq
    hardware_timer   
)

# Create map/bin/hex/uf2 file in addition to ELF
pico_add_extra_outputs(${NAME})
