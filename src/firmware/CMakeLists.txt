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

project(BT-USB-ADB-Adapter-firmware VERSION 1.0.16)

# Single source of truth for firmware version (bump when releasing; used in banner and docs)
add_compile_definitions(BT_USB_ADB_ADAPTER_VERSION_STRING=\"${PROJECT_VERSION}\")

# ADB serial debug: set -DADB_DEBUG=ON when configuring to get ADB timing/command messages on UART
option(ADB_DEBUG "Enable ADB serial debug output (UART)" OFF)
if(ADB_DEBUG)
    add_compile_definitions(ADB_DEBUG=1)
    message(STATUS "ADB debug: ENABLED (serial output)")
else()
    message(STATUS "ADB debug: disabled")
endif()

# Mouse movement handling between ADB polls:
# ON  -> accumulate USB/BLE deltas (smoother on some IIgs apps)
# OFF -> legacy overwrite (latest delta only)
option(ADB_MOUSE_ACCUMULATE_DELTAS "Accumulate mouse deltas between ADB polls" ON)
if(ADB_MOUSE_ACCUMULATE_DELTAS)
    message(STATUS "ADB mouse delta accumulation: ENABLED")
else()
    message(STATUS "ADB mouse delta accumulation: disabled")
endif()

# IIgs Table 6-8 attention 560–1040 µs; default 500 µs adds RP2040 measurement slack.
# A/B: same firmware version, different floor — e.g. -DADB_ATTENTION_LO_MIN_US=450
set(ADB_ATTENTION_LO_MIN_US 500 CACHE STRING
    "Minimum ADB attention low time (microseconds) for ReceiveCommand")
message(STATUS "ADB_ATTENTION_LO_MIN_US=${ADB_ATTENTION_LO_MIN_US}")

# Phase-B decode alignment: strict duty-cycle decode per IIgs guide
# ON  -> bit 1 when low <35% of cell, bit 0 when low >65%, else reject bit
# OFF -> midpoint decode (legacy/tolerant)
option(ADB_STRICT_DUTY_CYCLE_DECODE "Use strict 35%/65% duty-cycle decode for ADB receive bits" OFF)
if(ADB_STRICT_DUTY_CYCLE_DECODE)
    message(STATUS "ADB strict duty-cycle decode: ENABLED (35%/65%)")
else()
    message(STATUS "ADB strict duty-cycle decode: disabled (midpoint)")
endif()

# Phase-C sync envelope: IIgs spec envelope is 42-91 us (60-70% of 70-130 us cell).
# OFF keeps the current tolerant 40-95 us window.
option(ADB_STRICT_SYNC_WINDOW "Use strict sync window (42-91 us) in ReceiveCommand" OFF)
if(ADB_STRICT_SYNC_WINDOW)
    message(STATUS "ADB strict sync window: ENABLED (42-91 us)")
else()
    message(STATUS "ADB strict sync window: disabled (40-95 us)")
endif()

# IIgs policy: mouse must not assert SRQ.
# When enabled, we suppress mouse SRQ extension so the host doesn't spend extra time
# servicing mouse service requests in some GUIs. Default ON for all builds; use
# -DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF for legacy behavior (mouse may extend SRQ).
option(ADB_IIGS_MOUSE_SUPPRESS_SRQ "Suppress ADB SRQ extension for mouse (keyboard only)" ON)
if(ADB_IIGS_MOUSE_SUPPRESS_SRQ)
    message(STATUS "ADB mouse SRQ suppression: ENABLED (keyboard only)")
else()
    message(STATUS "ADB mouse SRQ suppression: disabled (kbd or mouse)")
endif()

if(ADB_IIGS_MOUSE_SUPPRESS_SRQ)
    add_compile_definitions(ADB_IIGS_MOUSE_SUPPRESS_SRQ=1)
else()
    add_compile_definitions(ADB_IIGS_MOUSE_SUPPRESS_SRQ=0)
endif()

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
# Display (SSD1306) headers for quokkadb.cpp
target_include_directories(QuokkADB PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_subdirectory(lib/usb/src)

# Bluepad32: add init/platform sources, includes, and link (wireless boards only)
if(ENABLE_BLUEPAD32)
    add_subdirectory(${CMAKE_SOURCE_DIR}/bluepad32/src/components/bluepad32 libbluepad32)
    target_include_directories(bluepad32 PRIVATE ${CMAKE_SOURCE_DIR}/src)

    target_sources(BT-USB-ADB-Adapter-firmware PRIVATE
        ${CMAKE_SOURCE_DIR}/src/bluepad32_init.c
        ${CMAKE_SOURCE_DIR}/src/bluepad32_platform.c
    )
    target_include_directories(BT-USB-ADB-Adapter-firmware PRIVATE
        ${CMAKE_SOURCE_DIR}/bluepad32/src/components/bluepad32/include
        ${CMAKE_SOURCE_DIR}/src
        ${PICO_SDK_PATH}/lib/btstack/src
        ${PICO_SDK_PATH}/src/boards/include
    )
    # CYW43 driver expects CYW43_DEFAULT_* from board header; define for pico_w/pico2_w
    if(PICO_BOARD STREQUAL "pico_w" OR PICO_BOARD STREQUAL "pico2_w")
        target_compile_definitions(BT-USB-ADB-Adapter-firmware PRIVATE
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
    target_link_libraries(BT-USB-ADB-Adapter-firmware
        pico_cyw43_arch_none
        pico_btstack_ble
        pico_btstack_classic
        pico_btstack_cyw43
        pico_async_context_poll
        bluepad32
    )
endif()

