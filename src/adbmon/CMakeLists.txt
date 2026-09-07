cmake_minimum_required(VERSION 3.22)

include(pico_sdk_import.cmake)

project(adbmon VERSION 0.2.3)

add_compile_definitions(ADBMON_VERSION_STRING=\"${PROJECT_VERSION}\")

option(ADBMON_STRICT_DECODE "Use IIgs strict duty-cycle and sync windows" OFF)
if(ADBMON_STRICT_DECODE)
    add_compile_definitions(ADBMON_STRICT_DUTY_CYCLE_DECODE=1 ADBMON_STRICT_SYNC_WINDOW=1)
    message(STATUS "adbmon strict decode: ENABLED")
else()
    add_compile_definitions(ADBMON_STRICT_DUTY_CYCLE_DECODE=0 ADBMON_STRICT_SYNC_WINDOW=0)
    message(STATUS "adbmon strict decode: disabled (tolerant)")
endif()

set(ADB_ATTENTION_LO_MIN_US 500 CACHE STRING "Minimum attention low time (microseconds)")
add_compile_definitions(ADB_ATTENTION_LO_MIN_US=${ADB_ATTENTION_LO_MIN_US})
message(STATUS "ADB_ATTENTION_LO_MIN_US=${ADB_ATTENTION_LO_MIN_US}")

pico_sdk_init()

add_executable(adbmon
    src/main.cpp
    src/adbmon_gpio.c
    src/adbmon_bus.cpp
    src/adbmon_log.cpp
)

target_include_directories(adbmon PRIVATE include)
target_link_libraries(adbmon pico_stdlib hardware_gpio)

pico_enable_stdio_usb(adbmon 1)
pico_enable_stdio_uart(adbmon 1)
pico_add_extra_outputs(adbmon)
