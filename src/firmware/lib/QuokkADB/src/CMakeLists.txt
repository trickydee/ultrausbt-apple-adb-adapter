
add_library(QuokkADB
    adb_platform.cpp
    bt_hid_bridge.cpp
    flashsettings.cpp
    platformkbdparser.cpp
    platformmouseparser.cpp
    quokkadb_gpio.cpp
    quokkadb.cpp
    rp2040_serial.cpp
    usbhost.cpp
    ${CMAKE_SOURCE_DIR}/src/usb_device_map.c
    ${CMAKE_SOURCE_DIR}/src/adb_hub.c
)

target_link_libraries(QuokkADB pico_stdlib hardware_pio hardware_flash hardware_clocks pico_multicore adb usb tinyusb_common tinyusb_host)
target_include_directories(QuokkADB PUBLIC ../include)

if(ADB_IIGS_MOUSE_SUPPRESS_SRQ)
    target_compile_definitions(QuokkADB PRIVATE ADB_IIGS_MOUSE_SUPPRESS_SRQ=1)
else()
    target_compile_definitions(QuokkADB PRIVATE ADB_IIGS_MOUSE_SUPPRESS_SRQ=0)
endif()

if(ADB_PASSTHROUGH_HUB_DEFAULT)
    target_compile_definitions(QuokkADB PRIVATE ADB_PASSTHROUGH_HUB_DEFAULT=1)
else()
    target_compile_definitions(QuokkADB PRIVATE ADB_PASSTHROUGH_HUB_DEFAULT=0)
endif()