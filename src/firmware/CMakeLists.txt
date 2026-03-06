cmake_minimum_required(VERSION 3.22)

set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/boards)
# Use upstream TinyUSB from submodule if present (see docs/changes.md)
if (EXISTS ${CMAKE_SOURCE_DIR}/tinyusb/hw/bsp/rp2040)
    set(PICO_TINYUSB_PATH "${CMAKE_SOURCE_DIR}/tinyusb" CACHE PATH "Path to TinyUSB (upstream)")
    message(STATUS "Using upstream TinyUSB at ${PICO_TINYUSB_PATH}")
endif ()
# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(HIDHopper-firmware VERSION 1.0.2)

# Single source of truth for firmware version (bump when releasing; used in banner and docs)
add_compile_definitions(HIDHOPPER_ADB_VERSION_STRING=\"${PROJECT_VERSION}\")

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Bluepad32: enable only for wireless boards (Pico W / Pico 2 W)
if(PICO_BOARD STREQUAL "pico_w" OR PICO_BOARD STREQUAL "pico2_w")
    set(ENABLE_BLUEPAD32 ON)
    add_compile_definitions(ENABLE_BLUEPAD32=1)
    message(STATUS "Bluepad32: ENABLED (${PICO_BOARD})")
else()
    set(ENABLE_BLUEPAD32 OFF)
    add_compile_definitions(ENABLE_BLUEPAD32=0)
    message(STATUS "Bluepad32: DISABLED (${PICO_BOARD})")
endif()

# rest of your project
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Os -DQUOKKADB -DSCQ_RP2040_MUTEX ")

target_include_directories(tinyusb_common INTERFACE include)

add_subdirectory(src)
add_subdirectory(lib/adb/src)
add_subdirectory(lib/QuokkADB/src)
add_subdirectory(lib/usb/src)

# Bluepad32: add init/platform sources, includes, and link (wireless boards only)
if(ENABLE_BLUEPAD32)
    add_subdirectory(${CMAKE_SOURCE_DIR}/bluepad32/src/components/bluepad32 libbluepad32)
    target_include_directories(bluepad32 PRIVATE ${CMAKE_SOURCE_DIR}/src)

    target_sources(HIDHopper-firmware PRIVATE
        ${CMAKE_SOURCE_DIR}/src/bluepad32_init.c
        ${CMAKE_SOURCE_DIR}/src/bluepad32_platform.c
    )
    target_include_directories(HIDHopper-firmware PRIVATE
        ${CMAKE_SOURCE_DIR}/bluepad32/src/components/bluepad32/include
        ${CMAKE_SOURCE_DIR}/src
        ${PICO_SDK_PATH}/lib/btstack/src
        ${PICO_SDK_PATH}/src/boards/include
    )
    # CYW43 driver expects CYW43_DEFAULT_* from board header; define for pico_w/pico2_w
    if(PICO_BOARD STREQUAL "pico_w" OR PICO_BOARD STREQUAL "pico2_w")
        target_compile_definitions(HIDHopper-firmware PRIVATE
            CYW43_DEFAULT_PIN_WL_REG_ON=23
            CYW43_DEFAULT_PIN_WL_DATA_OUT=24
            CYW43_DEFAULT_PIN_WL_DATA_IN=24
            CYW43_DEFAULT_PIN_WL_HOST_WAKE=24
            CYW43_DEFAULT_PIN_WL_CLOCK=29
            CYW43_DEFAULT_PIN_WL_CS=25
            CYW43_WL_GPIO_LED_PIN=0
        )
    endif()
    target_include_directories(QuokkADB PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(HIDHopper-firmware
        pico_cyw43_arch_none
        pico_btstack_ble
        pico_btstack_classic
        pico_btstack_cyw43
        pico_async_context_poll
        bluepad32
    )
endif()

